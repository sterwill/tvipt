require Logger

defmodule Tvipt.ConnectionReader do
  use GenServer

  def start_link(client, conn_pid) do
    GenServer.start_link(__MODULE__, %{client: client, conn_pid: conn_pid})
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

  def terminate(_reason, state) do
    Logger.info("arf")
    Tvipt.Connection.stop(state[:conn_pid])
    :ignored
  end

  def stop(pid) do
    GenServer.cast(pid, {:stop})
  end

  def handle_cast({:read}, state) do
    client = state[:client]
    conn_pid = state[:conn_pid]

    # Do a blocking recv for a short time so we can handle external stop messages after the read
    case :gen_tcp.recv(client, 0, 100) do
      {:ok, data} ->
        # Send the data to the connection process and schedule another read
        GenServer.cast(conn_pid, {:reader_data, data})
        GenServer.cast(self(), {:read})
        {:noreply, state}
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
end
