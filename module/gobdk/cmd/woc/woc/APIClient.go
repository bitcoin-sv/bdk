package woc

import (
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"time"
)

const (
	Mainnet = "main"
	Testnet = "test"
	STNnet  = "stn"
)

// RateLimit hold the definition of rate limite for whatsonchain
// See
//
//	https://platform.taal.com/plans/individual-services?plan=woc
type RateLimit struct {
	NbRequestSecond int `mapstructure:"nbRequestSecond" json:"nbRequestSecond" validate:"required"`
	NbRequestDay    int `mapstructure:"nbRequestDay" json:"nbRequestDay" validate:"required"`
}

func DefaultRateLimit() RateLimit {
	return RateLimit{NbRequestSecond: 3, NbRequestDay: 100_000}
}

type APIClient struct {
	counter     uint64
	network     string
	rateLimit   RateLimit
	tQueue      RingQueue[time.Time]
	debugLogger *log.Logger
}

func NewAPIClient(n string, r RateLimit) *APIClient {

	client := &APIClient{
		counter:     uint64(0),
		network:     n,
		rateLimit:   r,
		debugLogger: log.New(os.Stdout, "[DEBUG RATELIMIT] ", log.Ldate|log.Ltime|log.Lmicroseconds),
	}

	// Initialize time tracker queue
	client.tQueue = *NewRingQueue[time.Time](client.rateLimit.NbRequestSecond)
	now := time.Now()
	for i := 0; i < client.rateLimit.NbRequestSecond; i++ {
		client.tQueue.Push(now)
	}

	return client
}

// BaseURL return the base url of the api
func (c *APIClient) baseURL() string {
	return fmt.Sprintf("https://api.whatsonchain.com/v1/bsv/%v", c.network)
}

// Fetch hit the url and return the result as json string
func (c *APIClient) Fetch(path string) (string, error) {
	c.counter += uint64(1)
	url := fmt.Sprintf("%v/%v", c.baseURL(), path)

	firstTime, err := c.tQueue.Pop()
	if err != nil {
		return "", fmt.Errorf("unable to pop the time tracker queue, error %w", err)
	}

	// Make sleep to pass the 1 second rate limite
	elapsed := time.Since(firstTime)
	//c.debugLogger.Printf("[%8d] elapsed : %v ms, URL : %v", c.counter, elapsed.Microseconds(), url)
	if elapsed < time.Second {
		sleepDuration := time.Second - elapsed
		time.Sleep(sleepDuration)
	}

	// Make the GET request
	httpClient := &http.Client{}
	resp, err := httpClient.Get(url)
	if err != nil {
		return "", fmt.Errorf("failed to get the query\nURL : %v\nerror\n%w", url, err)
	}
	defer resp.Body.Close()

	if err := c.tQueue.Push(time.Now()); err != nil {
		return "", fmt.Errorf("unable to push the time tracker queue, error %w", err)
	}

	// Check if the request was successful (status code 200)
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("failed to get the query\nURL : %v\nStatusCode : %v", url, resp.StatusCode)
	}

	// Read the response body
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("failed to get response body\nURL : %v\nerror\n%w", url, err)
	}

	return string(body), nil
}
