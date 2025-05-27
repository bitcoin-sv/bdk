package script

/*
#cgo CFLAGS: -I${SRCDIR}/..
#include <bdkcgo/gobdk.h>
*/
import "C"

import (
	"fmt"

	_ "github.com/bitcoin-sv/bdk/module/gobdk/bdkcgo"
)

// Define a custom type for the enum
type ConsensusErrorCode int

// Declare the enum values using iota
const (
	BITCOINCONSENSUS_ERR_OK ConsensusErrorCode = iota // In go binding, equivalent of C++ BITCOINCONSENSUS_ERR_OK is error nil in golang
	BITCOINCONSENSUS_ERR_TX_INDEX
	BITCOINCONSENSUS_ERR_TX_SIZE_MISMATCH
	BITCOINCONSENSUS_ERR_TX_DESERIALIZE
	BITCOINCONSENSUS_ERR_AMOUNT_REQUIRED
	BITCOINCONSENSUS_ERR_INVALID_FLAGS
)

// ConsensusError is the returned result when checking the bitcoin consensus for a script
type ConsensusError interface {
	error
	Code() ConsensusErrorCode
	SetInternal(err error)
}

// NewConsensusError create a new consensus error
func NewConsensusError(code ConsensusErrorCode) ConsensusError {
	return &ConsensusErrorImpl{
		errCode: code,
	}
}

// ScriptErrorImpl implement the interface ScriptError
type ConsensusErrorImpl struct {
	errCode  ConsensusErrorCode `json:"-"`
	internal error              `json:"-"` // Stores the error returned by an external dependency
}

// Code implement the standard error interface.
func (se *ConsensusErrorImpl) Code() ConsensusErrorCode {
	return se.errCode
}

// Error implement the standard error interface.
func (se *ConsensusErrorImpl) Error() string {
	if se.internal == nil {
		return consensusErrorCode2String(se.errCode)
	}

	errCode := se.Code()
	errCodeStr := consensusErrorCode2String(se.errCode)
	errStr := fmt.Sprintf("code=%d, message=%v, internal=%v", errCode, errCodeStr, se.internal.Error())

	return errStr
}

// Unwrap implement the standard error wrapper interface.
func (se *ConsensusErrorImpl) Unwrap() error {
	return se.internal
}

// SetInternal sets error to ConsensusErrorImpl.internal, to be unwrapped later
func (se *ConsensusErrorImpl) SetInternal(err error) {
	se.internal = err
}

// consensusErrorCode2String conversion of ConsensusErrorCode to string
func consensusErrorCode2String(e ConsensusErrorCode) string {
	switch e {
	case BITCOINCONSENSUS_ERR_OK:
		return "OK"
	case BITCOINCONSENSUS_ERR_TX_INDEX:
		return "invalid or mismatch indices"
	case BITCOINCONSENSUS_ERR_TX_SIZE_MISMATCH:
		return "invalid transaction size"
	case BITCOINCONSENSUS_ERR_TX_DESERIALIZE:
		return "unable to deserialize transaction"
	case BITCOINCONSENSUS_ERR_AMOUNT_REQUIRED:
		return "transaction missing amount"
	case BITCOINCONSENSUS_ERR_INVALID_FLAGS:
		return "invalid flags"
	default:
		return "unknown error, might be an exception in CGO"
	}
}
