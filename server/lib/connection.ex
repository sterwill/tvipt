require Logger
require Exexec
require Kcl
require Tvipt.ConnectionReader

defmodule Tvipt.Connection do
  use GenServer

  def start(client, shell_cmd, secret_key) do
    GenServer.start(__MODULE__, %{client: client, shell_cmd: shell_cmd, secret_key: secret_key})
  end

  def init(args) do
    {:ok, args}
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
    Logger.info("terminating: #{inspect(reason)}")
    Exexec.stop(state[:exec_pid])
    :ignored
  end

  def handle_cast(:serve, state) do
    Logger.info("connection at #{inspect(self())}")
    client = state[:client]

    {:ok, reader_pid} = Tvipt.ConnectionReader.start_link(client, self(), state[:secret_key])
    Logger.info("reader at #{inspect(reader_pid)}")

    {:ok, exec_pid, os_pid} = Exexec.run(
      state[:shell_cmd],
      stdin: true,
      stdout: true,
      stderr: true,
      pty: true,
      monitor: true
    )
    Logger.info("exec at #{inspect(exec_pid)}, OS PID #{os_pid}")

    state = state
            |> Map.put(:exec_pid, exec_pid)
            |> Map.put(:reader_pid, reader_pid)

    {:noreply, state}
  end

  # Handle data read from the network connection
  def handle_cast({:read_client_msg, msg}, state) do
    # Decode the message
    <<_size :: binary - size(2), nonce :: binary - size(24), ciphertext :: binary>> = msg

    # Decrypt the message
    plaintext = Kcl.secretunbox(ciphertext, nonce, state[:secret_key])

    # Send the plaintext to the shell
    Exexec.send(state[:exec_pid], plaintext)

    {:noreply, state}
  end

  # Handle stdout read from the shell
  def handle_info({:stdout, _os_pid, data}, state) do
    send_to_client(data, state)
    {:noreply, state}
  end

  # Handle stderr read from the shell
  def handle_info({:stderr, _os_pid, data}, state) do
    send_to_client(data, state)
    {:noreply, state}
  end

  #  Handle the shell process stopping
  def handle_info({:DOWN, _os_pid, :process, exec_pid, _reason}, _state) do
    Logger.info("exec at #{inspect(exec_pid)} closed, stopping connection")
    {:stop, :shell_exit}
  end

  defp send_to_client(data, state) do
    nonce = :crypto.strong_rand_bytes(24)
    box = Kcl.secretbox(data, nonce, state[:secret_key])
    :gen_tcp.send(state[:client], box)
  end
end