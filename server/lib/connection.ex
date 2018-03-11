require Logger
require Exexec
require Tvipt.ConnectionReader
require Chacha20
require Poly1305

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
  def handle_cast({:read_client_msg, nonce, tag, ciphertext}, state) do
    Logger.debug("recv nonce: #{inspect(nonce, base: :hex)}")
    Logger.debug("recv tag: #{inspect(tag, base: :hex)}")
    Logger.debug("recv ciphertext: #{inspect(ciphertext, base: :hex)}")

    case Poly1305.aead_decrypt(ciphertext, state[:secret_key], nonce, tag) do
      :error ->
        Logger.info("decrypt error")
        {:stop, :decrypt_error}
      plaintext ->
        # Send the plaintext to the shell
        Exexec.send(state[:exec_pid], plaintext)
        {:noreply, state}
    end
  end

  # Handle stdout read from the shell
  def handle_info({:stdout, _os_pid, data}, state)  do
    send_to_client(data, state)
    {:noreply, state}
  end

  # Handle stderr read from the shell
  def handle_info({:stderr, _os_pid, data}, state)  do
    send_to_client(data, state)
    {:noreply, state}
  end

  #  Handle the shell process stopping
  def handle_info({:DOWN, _os_pid, :process, exec_pid, _reason}, _state) do
    Logger.info("exec at #{inspect(exec_pid)} closed, stopping connection")
    {:stop, :shell_exit}
  end

  defp send_to_client(<<>>, _state) do
    :ok
  end

  defp send_to_client(data, state) when byte_size(data) > 255 do
    send_to_client(binary_part(data, 0, 255), state)
    send_to_client(binary_part(data, 255, byte_size(data) - 255), state)
  end

  defp send_to_client(data, state) when byte_size(data) <= 255 do
    nonce = :crypto.strong_rand_bytes(12)
    {ciphertext, tag} = Poly1305.aead_encrypt(data, state[:secret_key], nonce)
    Logger.debug("send nonce: #{inspect(nonce, base: :hex)}")
    Logger.debug("send tag: #{inspect(tag, base: :hex)}")
    Logger.debug("send ciphertext len: #{inspect(byte_size(ciphertext), base: :hex)}")
    Logger.debug("send ciphertext: #{inspect(ciphertext, base: :hex)}")
    :gen_tcp.send(state[:client], nonce <> tag <> <<byte_size(ciphertext) :: integer - size(8)>> <> ciphertext)
    :ok
  end
end