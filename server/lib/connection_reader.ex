require Logger

defmodule Tvipt.ConnectionReader do
  use GenServer

  def start_link(client, conn_pid, secret_key) do
    GenServer.start_link(__MODULE__, %{client: client, conn_pid: conn_pid, secret_key: secret_key, msg_buf: <<>>})
  end

  def init(state) do
    client = state[:client]

    # Turn off receive buffering so we can handle single chars
    case :inet.setopts(client, [buffer: 0, recbuf: 0]) do
      :ok ->
        # Start reading
        GenServer.cast(self(), {:read})
        {:ok, state}
      {:error, reason} ->
        {:stop, :setopt_error, reason}
    end
  end

  def terminate(reason, state) do
    Logger.info("terminating: #{inspect(reason)}")
    Tvipt.Connection.stop(state[:conn_pid])
    :ignored
  end

  def stop(pid) do
    GenServer.cast(pid, {:stop})
  end

  def handle_cast({:read}, state) do
    client = state[:client]

    # Do a blocking recv for a short time so we can handle external stop messages after the read
    case :gen_tcp.recv(client, 0, 100) do
      {:ok, data} ->
        {:noreply, handle_recv(data, state)}
      {:error, :timeout} ->
        # Schedule another read after handling any queued messages
        GenServer.cast(self(), {:read})
        {:noreply, state}
      {:error, :closed} ->
        # Stop this process
        {:stop, :connection_closed, state}
    end
  end

  def handle_cast({:stop}, state) do
    GenServer.stop(self())
    {:stop, :external_stop, state}
  end

  defp handle_recv(data, state) do
    msg_buf = state[:msg_buf] <> data

    msg_buf = case byte_size(msg_buf) > 0 do
      true ->
        <<msg_size :: integer - size(8), _ :: binary>> = msg_buf
        msg_total_size = msg_size + 1

        case byte_size(msg_buf) >= msg_total_size do
          true ->
            # Got a full message
            <<msg :: (binary - size (msg_total_size)), msg_buf_remainder :: binary>> = msg_buf
            GenServer.cast(state[:conn_pid], {:read_client_msg, msg})
            msg_buf_remainder
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
