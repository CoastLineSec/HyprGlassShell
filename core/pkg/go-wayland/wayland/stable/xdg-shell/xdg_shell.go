package xdg_shell

import "github.com/CoastLineSec/HyprGlassShell/core/pkg/go-wayland/wayland/client"

type Popup struct {
	client.BaseProxy
}

func NewPopup(ctx *client.Context) *Popup {
	p := &Popup{}
	ctx.Register(p)
	return p
}
