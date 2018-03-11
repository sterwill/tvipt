require Logger
require Tvipt.Server

defmodule Tvipt.App do
  use Application

  def start(_type, _args) do
    port = Application.fetch_env!(:tvipt, :port)
    secret_key = Application.fetch_env!(:tvipt, :secret_key)
    shell_cmd = Application.fetch_env!(:tvipt, :shell_cmd)

    children = [
      %{
        id: Tvipt.Server,
        start: {Tvipt.Server, :start_link, [%{port: port, shell_cmd: shell_cmd, secret_key: secret_key}]}
      }
    ]
    Supervisor.start_link(children, [name: :tvipt_app, strategy: :one_for_one])
  end
end
