module github.com/bitcoin-sv/bdk/test/golang

go 1.23.0

toolchain go1.24.1

require (
	github.com/bitcoin-sv/bdk/module/gobdk v0.0.0-20240904084246-cb0e25691dc9
	github.com/stretchr/testify v1.10.0
)

require (
	github.com/kr/text v0.2.0 // indirect
	github.com/libsv/go-bk v0.1.6 // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/rogpeppe/go-internal v1.9.0 // indirect
	golang.org/x/crypto v0.35.0 // indirect
)

require (
	github.com/davecgh/go-spew v1.1.2-0.20180830191138-d8f796af33cc // indirect
	github.com/libsv/go-bt/v2 v2.2.5
	github.com/pmezard/go-difflib v1.0.1-0.20181226105442-5d4384ee4fb2 // indirect
	gopkg.in/yaml.v3 v3.0.1 // indirect
)

replace github.com/bitcoin-sv/bdk/module/gobdk => ../../module/gobdk
