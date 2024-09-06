module github.com/bitcoin-sv/bdk/test/golang

go 1.22.6

require (
	github.com/bitcoin-sv/bdk/module/gobdk v0.0.0-20240904084246-cb0e25691dc9
	github.com/stretchr/testify v1.9.0
)

require (
	github.com/davecgh/go-spew v1.1.1 // indirect
	github.com/pmezard/go-difflib v1.0.0 // indirect
	gopkg.in/yaml.v3 v3.0.1 // indirect
)

replace github.com/bitcoin-sv/bdk/module/gobdk => ../../module/gobdk
