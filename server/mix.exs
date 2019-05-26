defmodule Server.Mixfile do
  use Mix.Project

  def project do
    [
      app: :tvipt,
      version: "0.1.0",
      elixir: "~> 1.5",
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger, :porcelain],
      mod: {Tvipt.App, []}
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
      {:porcelain, "~> 2.0"},
      {:chacha20, "~> 1.0"},
      {:poly1305, "~> 1.0"}
    ]
  end
end
