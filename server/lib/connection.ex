defmodule Tvipt.Connection do
  use GenServer

  require Logger

  defmodule State do
    defstruct [:client, :program, :program_args, :secret_key, :reader_pid, :porcelain_proc]
  end

  # Client

  def start(args) do
    GenServer.start(__MODULE__, args)
  end

  def serve(pid) do
    GenServer.cast(pid, :serve)
    :ok
  end

  def stop(pid, reason) do
    GenServer.stop(pid, reason)
  end

  # Server

  @impl true
  def init(args) do
    state = struct(State, args)
    {:ok, state}
  end

  @impl true
  def handle_cast(:serve, state) do
    client = state.client

    reader_args = [client: client, conn_pid: self(), secret_key: state.secret_key]
    {:ok, reader_pid} = Tvipt.ConnectionReader.start_link(reader_args)
    Logger.info("started reader #{inspect(reader_pid)}")

    porcelain_proc =
      Porcelain.spawn(
        state.program,
        state.program_args,
        in: :receive,
        out: {:send, self()},
        err: :out,
        result: :keep
      )

    # Link the process to this one so the shell gets stopped if the connection drops
    # for any reason
    Process.link(porcelain_proc.pid)

    Logger.info("started porcelain #{inspect(porcelain_proc.pid)}")

    {:noreply, %{state | reader_pid: reader_pid, porcelain_proc: porcelain_proc}}
  end

  # Handle data read from the network connection
  @impl true
  def handle_cast({:read_client_msg, nonce, tag, ciphertext}, state) do
    Logger.debug("recv nonce: #{inspect(nonce, base: :hex)}")
    Logger.debug("recv tag: #{inspect(tag, base: :hex)}")
    Logger.debug("recv ciphertext: #{inspect(ciphertext, base: :hex)}")

    case Poly1305.aead_decrypt(ciphertext, state.secret_key, nonce, tag) do
      :error ->
        Logger.info("decrypt error")
        {:stop, :decrypt_error, state}

      plaintext ->
        # Send the plaintext to the shell
        Porcelain.Process.send_input(state.porcelain_proc, plaintext)
        {:noreply, state}
    end
  end

  # Handle stdout read from the shell
  @impl true
  def handle_info({_porcelain_pid, :data, :out, data}, state) do
    send_to_client(data, state)
    {:noreply, state}
  end

  # Handle the shell process result
  @impl true
  def handle_info({porcelain_pid, :result, result}, state) do
    Logger.info("porcelain #{inspect(porcelain_pid)} finished with status #{result.status}")
    {:stop, :normal, state}
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
    {ciphertext, tag} = Poly1305.aead_encrypt(data, state.secret_key, nonce)
    Logger.debug("send nonce: #{inspect(nonce, base: :hex)}")
    Logger.debug("send tag: #{inspect(tag, base: :hex)}")
    Logger.debug("send ciphertext len: #{inspect(byte_size(ciphertext), base: :hex)}")
    Logger.debug("send ciphertext: #{inspect(ciphertext, base: :hex)}")

    :gen_tcp.send(
      state.client,
      nonce <> tag <> <<byte_size(ciphertext)::integer-size(8)>> <> ciphertext
    )

    :ok
  end
end
