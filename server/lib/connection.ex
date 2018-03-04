require Logger
require Exexec
require Tvipt.ConnectionReader

defmodule Tvipt.Connection do
  use GenServer

  def start(client) do
    GenServer.start(__MODULE__, %{client: client})
  end

  def serve(pid) do
    GenServer.cast(pid, :serve)
    :ok
  end

  def stop(pid) do
    # Crash the process for a quick exit, since its message queue may be large
    # (queued output from the process ready to be written to a slow network connection).
    Process.exit(pid, :stop)
  end

  def terminate(reason, state) do
    Logger.info("term")
    Exexec.stop(state[:exec_pid])
    :ignored
  end

  def handle_cast(:serve, state) do
    Logger.info("connection at #{inspect(self())}")
    client = state[:client]

    {:ok, reader_pid} = Tvipt.ConnectionReader.start_link(client, self())
    Logger.info("reader at #{inspect(reader_pid)}")

    shell_cmd = ["/bin/bash", "-l", "-i", "-c", "stty echo ; exec bash"]

    {:ok, exec_pid, os_pid} = Exexec.run(
      shell_cmd,
      stdin: true,
      stdout: true,
      stderr: true,
      pty: true,
      monitor: true,
    )
    Logger.info("exec at #{inspect(exec_pid)}, OS PID #{os_pid}")

    state = state
            |> Map.put(:exec_pid, exec_pid)
            |> Map.put(:reader_pid, reader_pid)

    Logger.info("shell operating system PID is #{os_pid}")
    {:noreply, state}
  end

  # Handle data read from the network connection
  def handle_cast({:reader_data, data}, state) do
    Exexec.send(state[:exec_pid], data)
    {:noreply, state}
  end

  # Handle stdout read from the shell
  def handle_info({:stdout, _os_pid, data}, state) do
    :gen_tcp.send(state[:client], data)
    {:noreply, state}
  end

  # Handle stderr read from the shell
  def handle_info({:stderr, _os_pid, data}, state) do
    :gen_tcp.send(state[:client], data)
    {:noreply, state}
  end

  #  Handle the shell process stopping
  def handle_info({:DOWN, _os_pid, :process, exec_pid, reason}, state) do
    Logger.info("exec at #{inspect(exec_pid)} closed, stopping connection")
    {:stop, :shell_exit}
  end
end