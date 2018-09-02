defmodule Tvipt.ConnectionReader do
  use GenServer

  require Logger

  defmodule State do
    defstruct [:client, :conn_pid, :secret_key, msg_buf: <<>>]
  end

  # Client

  def start_link(args) do
    GenServer.start_link(__MODULE__, args)
  end

  def stop(pid, reason) do
    GenServer.stop(pid, reason)
  end

  # Server

  @impl true
  def init(args) do
    state = struct(State, args)

    # Turn off receive buffering so we can handle single chars
    case :inet.setopts(state.client, buffer: 0, recbuf: 0) do
      :ok ->
        # Start reading
        GenServer.cast(self(), {:read})
        {:ok, state}

      {:error, reason} ->
        {:stop, :setopt_error, reason}
    end
  end

  @impl true
  def handle_cast({:read}, state) do
    # Do a blocking recv for a short time so we can handle external stop messages after the read
    case :gen_tcp.recv(state.client, 0, 100) do
      {:ok, data} ->
        {:noreply, handle_recv(data, state)}

      {:error, :timeout} ->
        # Schedule another read after handling any queued messages
        GenServer.cast(self(), {:read})
        {:noreply, state}

      {:error, :closed} ->
        # Stop this process
        Logger.info("connection closed")
        {:stop, :normal, state}
    end
  end

  defp handle_recv(data, state) do
    msg_buf = state.msg_buf <> data
    # Logger.debug("buf: #{inspect(msg_buf, base: :hex)}")

    msg_buf =
      case byte_size(msg_buf) > 12 + 16 + 1 do
        true ->
          # Buffer contains the nonce, tag, and ciphertext length of one message
          <<
            nonce::binary-size(12),
            tag::binary-size(16),
            ciphertext_len::integer-size(8),
            ciphertext_and_next_msg::binary
          >> = msg_buf

          case byte_size(msg_buf) >= 12 + 16 + 1 + ciphertext_len do
            true ->
              # Buffer contains the complete message including all ciphertext, and possibly
              # bytes for the next message
              <<
                ciphertext::binary-size(ciphertext_len),
                next_msg_buf::binary
              >> = ciphertext_and_next_msg

              GenServer.cast(state.conn_pid, {:read_client_msg, nonce, tag, ciphertext})
              next_msg_buf

            _ ->
              # Not enough bytes for the declared size
              msg_buf
          end

        _ ->
          # Not enough bytes for any message
          msg_buf
      end

    # Schedule another read
    GenServer.cast(self(), {:read})

    %{state | msg_buf: msg_buf}
  end
end
