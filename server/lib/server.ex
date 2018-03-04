require Supervisor
require Logger
require Tvipt.Connection
require Base

defmodule Tvipt.Server do
  use GenServer

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, [name: :tvipt_server])
  end

  def init(args) do
    GenServer.cast(self(), {:serve})
    {:ok, args}
  end

  def handle_cast({:serve}, state) do
    {:ok, socket} = :gen_tcp.listen(state[:port], [:binary, packet: :line, active: false, reuseaddr: true])
    Logger.info("listening on port #{state[:port]}")
    accept(socket, state[:secret_key])
    {:noreply, state}
  end

  defp accept(socket, secret_key) do
    {:ok, client} = :gen_tcp.accept(socket)
    {:ok, pid} = Tvipt.Connection.start(client, secret_key)
    :ok = :gen_tcp.controlling_process(client, pid)
    Tvipt.Connection.serve(pid)
    accept(socket, secret_key)
  end
end
