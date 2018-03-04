require Logger
require Tvipt.Server

defmodule Tvipt.App do
  use Application

  def start(_type, _args) do
    port = Application.fetch_env!(:tvipt, :port)
    secret_key_hex = Application.fetch_env!(:tvipt, :secret_key)
    secret_key = elem(Base.decode16(secret_key_hex), 1)

    children = [
      %{
        id: Tvipt.Server,
        start: {Tvipt.Server, :start_link, [%{port: port, secret_key: secret_key}]}
      }
    ]
    Supervisor.start_link(children, [name: :tvipt_app, strategy: :one_for_one])
  end
end
