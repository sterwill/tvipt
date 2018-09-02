defmodule Tvipt.App do
  use Application

  require Logger

  def start(_type, _args) do
    port = Application.fetch_env!(:tvipt, :port)
    secret_key = Application.fetch_env!(:tvipt, :secret_key)
    program = Application.fetch_env!(:tvipt, :program)
    program_args = Application.fetch_env!(:tvipt, :program_args)

    children = [
      %{
        id: Tvipt.Server,
        start:
          {Tvipt.Server, :start_link,
           [
             [
               port: port,
               program: program,
               program_args: program_args,
               secret_key: secret_key
             ]
           ]}
      }
    ]

    Supervisor.start_link(children, name: :tvipt_app, strategy: :one_for_one)
  end
end
