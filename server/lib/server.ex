defmodule Tvipt.Server do
  use GenServer

  require Logger

  defmodule State do
    @derive {Inspect, except: [:secret_key]}
    defstruct [:port, :program, :program_args, :secret_key]
  end

  def start_link(args) do
    GenServer.start_link(__MODULE__, args, name: :tvipt_server)
  end

  def init(args) do
    state = struct(State, args)
    GenServer.cast(self(), {:serve})
    {:ok, state}
  end

  def handle_cast({:serve}, state) do
    {:ok, socket} =
      :gen_tcp.listen(state.port, [:binary, packet: :line, active: false, reuseaddr: true])

    Logger.info("listening on port #{state.port}")
    accept(socket, state)
    {:noreply, state}
  end

  defp accept(socket, state) do
    {:ok, client} = :gen_tcp.accept(socket)

    connection_args = %{
      client: client,
      program: state.program,
      program_args: state.program_args,
      secret_key: state.secret_key
    }

    {:ok, pid} = Tvipt.Connection.start(connection_args)
    :ok = :gen_tcp.controlling_process(client, pid)
    Tvipt.Connection.serve(pid)
    accept(socket, state)
  end
end
