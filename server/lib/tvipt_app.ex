require Logger
require Tvipt.Server

defmodule Tvipt.App do
  use Application

  def start(_type, _args) do
    children = [
      %{
        id: Tvipt.Server,
        start: {Tvipt.Server, :start_link, [3333]}
      }
    ]
    Supervisor.start_link(children, [name: :tvipt_app, strategy: :one_for_one])
  end
end
