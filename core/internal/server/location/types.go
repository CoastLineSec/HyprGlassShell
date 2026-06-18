package location

import (
	"sync"

	"github.com/CoastLineSec/HyprGlassShell/core/internal/geolocation"
	"github.com/CoastLineSec/HyprGlassShell/core/pkg/syncmap"
)

type State struct {
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
}

type Manager struct {
	state      *State
	stateMutex sync.RWMutex

	client geolocation.Client

	stopChan chan struct{}
	sigWG    sync.WaitGroup

	subscribers  syncmap.Map[string, chan State]
	dirty        chan struct{}
	notifierWg   sync.WaitGroup
	lastNotified *State
}
