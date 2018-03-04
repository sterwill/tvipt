require Supervisor
require Logger
require Tvipt.Connection

defmodule Tvipt.Server do
  use GenServer

  def start_link(port) do
    GenServer.start_link(__MODULE__, port, [name: :tvipt_server])
  end

  def init(port) do
    GenServer.cast(self(), {:serve, port})
    {:ok, None}
  end

  def handle_cast({:serve, port}, state) do
    {:ok, socket} = :gen_tcp.listen(port, [:binary, packet: :line, active: false, reuseaddr: true])
    Logger.info("listening on port #{port}")
    accept(socket)
    {:noreply, state}
  end

  defp accept(socket) do
    {:ok, client} = :gen_tcp.accept(socket)
    {:ok, pid} = Tvipt.Connection.start(client)
    :ok = :gen_tcp.controlling_process(client, pid)
    Tvipt.Connection.serve(pid)
    accept(socket)
  end
end
