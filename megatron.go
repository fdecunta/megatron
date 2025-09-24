package main

import (
    "fmt"
    "log"
    "os"
    "os/exec"
    "path/filepath"
    "strings"

    "megatron/config"
    "megatron/filetree"
    "megatron/stack"

    "github.com/jroimartin/gocui"
)

var rootNode *filetree.Node
var currNode *filetree.Node
var selNode *filetree.Node
var leftItems []string
var rightItems []string

var currState stack.State
var stateStack stack.Stack

func main() {
    var rootDir string
    var err error

    if len(os.Args) > 1 {
        switch os.Args[1] {
            case "-c":
                config.EditConfig()
                os.Exit(0)
            case "-d":
                if len(os.Args) == 3 {
                    err := filetree.IsDir(os.Args[2])
                    if err != nil {
                        log.Fatal(err)
                    }
                    rootDir = os.Args[2]
                }
            default:
                usage()
                os.Exit(1)
        }
    } else { 
        rootDir, err = config.GetRootDir() 
        if err != nil {
            log.Fatal(err)
        }
    }

    rootNode, err = filetree.BuildTree(rootDir, nil)
    if err != nil {
        log.Fatal(err)
    }

    if len(rootNode.Children) == 0 {
        fmt.Printf("Directory is empty")
        os.Exit(1)
    }

    currNode = rootNode
    selNode = rootNode.Children[0]


    // Start TUI
    g, err := gocui.NewGui(gocui.OutputNormal)
    if err != nil {
        log.Panicln(err)
    }
    defer g.Close()


    // GUI managers and key bindings
    g.SetManagerFunc(layout)

    if err := g.SetKeybinding("", gocui.KeyCtrlC, gocui.ModNone, quit); err != nil {
        log.Panicln(err)
    }
    if err := g.SetKeybinding("", 'q', gocui.ModNone, quit); err != nil {
        log.Panicln(err)
    }
 	if err := g.SetKeybinding("", gocui.KeyArrowDown, gocui.ModNone, cursorDown); err != nil {
		log.Panicln(err)
	}
	if err := g.SetKeybinding("", gocui.KeyArrowUp, gocui.ModNone, cursorUp); err != nil {
		log.Panicln(err)
	}
 	if err := g.SetKeybinding("", 'j', gocui.ModNone, cursorDown); err != nil {
		log.Panicln(err)
	}
	if err := g.SetKeybinding("", 'k', gocui.ModNone, cursorUp); err != nil {
		log.Panicln(err)
	}
 	if err := g.SetKeybinding("", gocui.KeyArrowRight, gocui.ModNone, openNode); err != nil {
		log.Panicln(err)
	}
	if err := g.SetKeybinding("", gocui.KeyArrowLeft, gocui.ModNone, closeNode); err != nil {
		log.Panicln(err)
	}
    if err := g.SetKeybinding("", 'l', gocui.ModNone, openNode); err != nil {
    	log.Panicln(err)
    }
    if err := g.SetKeybinding("", 'h', gocui.ModNone, closeNode); err != nil {
    	log.Panicln(err)
    }

    if err := g.SetKeybinding("", gocui.KeyEnter, gocui.ModNone, openVideo); err != nil {
    	log.Panicln(err)
    }
    if err := g.SetKeybinding("", '?', gocui.ModNone, showHelp); err != nil {
        log.Panicln(err)
    }
    if err := g.SetKeybinding("", gocui.KeyCtrlF, gocui.ModNone, search); err != nil {
        log.Panicln(err)
    }

    currState = stack.State{0, 0}

    writeLeftItems()
    writeRightItems()

    if err := g.MainLoop(); err != nil && err != gocui.ErrQuit {
        log.Panicln(err)
    }
    
}

func usage() {
    fmt.Println("Usage: megatron [-c] [-d DIR]")
    fmt.Println("  -d  DIR  Open in dir")
    fmt.Println("  -c       Config default directory")
    fmt.Println("  -h       Help")
}


func layout(g *gocui.Gui) error {
    maxX, maxY := g.Size()

    t, err := g.SetView("title", 0, 0, maxX-1, 2)
    if err != nil && err != gocui.ErrUnknownView {
        return err
    }
    t.Clear()
    t.Frame = true
    drawCenteredTitle(t, "Megatron")

    stbar, err := g.SetView("status", 0, maxY-2, maxX-1, maxY)
    if err != nil && err != gocui.ErrUnknownView {
        return err
    }
    stbar.Clear()
    stbar.Frame = false
    fmt.Fprintln(stbar, "[jk|↑↓] Move  [lh|→←] Open/Close [Ctrl-F] Search  [Enter] Play  [q] Quit")


    l, err := g.SetView("left", 0, 3, maxX/2-1, maxY-2)
    if err != nil && err != gocui.ErrUnknownView {
        return err
    }
    l.Clear()
    l.Title = currNode.Path

    r, err := g.SetView("right", maxX/2, 3, maxX-1, maxY-2)
    if err != nil && err != gocui.ErrUnknownView {
        return err
    }
    r.Clear()
    if len(selNode.Children) == 0 {
        r.Title = ""
    } else {
        r.Title = filepath.Base(selNode.Path)
    }

	// Control visible portion
    _, rows := l.Size()
    if currState.Selected >= (currState.Origin+rows) {
        currState.Origin++
    } else if currState.Selected < currState.Origin {
        currState.Origin--
    }
    l.SetOrigin(0, currState.Origin)


    printLeftPanel(l)
    printRightPanel(r)

    return nil
}


func drawCenteredTitle(v *gocui.View, title string) {
    maxX, _ := v.Size()
    pad := (maxX - len(title)) / 2
    if pad < 0 {
        pad = 0
    }
    fmt.Fprintln(v, strings.Repeat(" ", pad)+title)
}


func printLeftPanel(l *gocui.View) {
    if len(leftItems) == 0 {
        fmt.Fprintf(l, "--Empty--")
    } else {
        for i, item := range leftItems  {
            if i == currState.Selected {
                fmt.Fprintf(l, "➤ \033[32m%s\033[0m\n", item) // green arrow
    		} else {
    			fmt.Fprintf(l, "  %s\n", item)
    		}
    	}
    }
}


func printRightPanel(r *gocui.View) {
    if len(rightItems) == 0 {
        fmt.Fprintf(r, "--Empty--")
    } else {
        for _, rItem := range rightItems {
            fmt.Fprintf(r, "  %s\n", rItem)
        }    
    }
}

func cursorDown(g *gocui.Gui, l *gocui.View) error {
    if currState.Selected < len(leftItems)-1 {
        currState.Selected++
        selNodeUpdate()
    }
    return nil
}

func cursorUp(g *gocui.Gui, l *gocui.View) error {
    if currState.Selected != 0 {
        currState.Selected--
        selNodeUpdate()
    }
    return nil
}

func selNodeUpdate() {
    selNode = currNode.Children[currState.Selected]
    rightItems = rightItems[:0]

    if len(selNode.Children) > 0 {
        writeRightItems()
    } 
}

func openNode(g *gocui.Gui, l *gocui.View) error {
    if len(selNode.Children) == 0 {
        return nil
    }

    stateStack.Push(currState)
    currNode = currNode.Children[currState.Selected]
    selNode = currNode.Children[0]

    currState.Selected = 0
    currState.Origin = 0

    writeLeftItems()
    writeRightItems()

    return nil
}


func closeNode(g *gocui.Gui, l *gocui.View) error {
    if stateStack.IsEmpty() {
        return nil
    }

    currState = stateStack.Top()
    stateStack.Pop()

    currNode = currNode.Parent
    selNode = currNode.Children[currState.Selected]

    writeLeftItems()
    writeRightItems()

    return nil
}

func writeLeftItems() {
    leftItems = leftItems[:0]
    for _, node := range currNode.Children {
        var pathStr string
        if node.IsDir {
            pathStr = fmt.Sprintf("\033[1m%s\033[0m", filepath.Base(node.Path))
        } else {
            pathStr = fmt.Sprintf("%s", filepath.Base(node.Path))
        }
        leftItems = append(leftItems, pathStr)
    }
}

func writeRightItems() {
    rightItems = rightItems[:0]
    for _, node := range selNode.Children {
        var pathStr string
        if node.IsDir {
            pathStr = fmt.Sprintf("\033[1m%s\033[0m", filepath.Base(node.Path))
        } else {
            pathStr = fmt.Sprintf("%s", filepath.Base(node.Path))
        }
        rightItems = append(rightItems, pathStr)
    }
}


func openVideo(g *gocui.Gui, l *gocui.View) error {
    if !IsVideo(selNode.Path) {
        return nil
    }

	cmd := exec.Command("vlc", "--fullscreen", selNode.Path)
    if err := cmd.Start(); err != nil {
        return err
    }
    return nil
}


func IsVideo(path string) bool {
	info, err := os.Stat(path)
	if err != nil {
		return false
	}

    // Is a regular file?
	if !info.Mode().IsRegular() {
		return false
	}

	// Looks like a video?
	ext := filepath.Ext(path)
	videoExts := map[string]bool{
		".mp4": true, ".mkv": true, ".avi": true,
		".mov": true, ".flv": true, ".wmv": true,
	}
	if videoExts[ext] {
		return true
	}
	return false
}

func quit(g *gocui.Gui, l *gocui.View) error {
    return gocui.ErrQuit
}

func showHelp(g *gocui.Gui, v *gocui.View) error {
    maxX, maxY := g.Size()
    if help, err := g.SetView("help", maxX/4, maxY/4, 3*maxX/4, 3*maxY/4); err != nil {
        if err != gocui.ErrUnknownView {
            return err
        }
        help.Title = "Help"
        fmt.Fprintln(help, "Navigation:")
        fmt.Fprintln(help, "↑/k, ↓/j - Move selection")
        fmt.Fprintln(help, "→/l, ←/h - Open/Close directory")
        fmt.Fprintln(help, "Enter - Play video")
        fmt.Fprintln(help, "q/Ctrl+C - Quit")
        
        if _, err := g.SetCurrentView("help"); err != nil {
            return err
        }

        // Bind "any key" to close help
        // Here we use rune(0) trick to catch *every rune*
        for r := rune(32); r <= 126; r++ { // printable ASCII
            g.SetKeybinding("help", r, gocui.ModNone, closeHelp)
        }
        // also catch arrows, Enter, etc.
        g.SetKeybinding("help", gocui.KeyArrowUp, gocui.ModNone, closeHelp)
        g.SetKeybinding("help", gocui.KeyArrowDown, gocui.ModNone, closeHelp)
        g.SetKeybinding("help", gocui.KeyArrowLeft, gocui.ModNone, closeHelp)
        g.SetKeybinding("help", gocui.KeyArrowRight, gocui.ModNone, closeHelp)
        g.SetKeybinding("help", gocui.KeyEnter, gocui.ModNone, closeHelp)
        g.SetKeybinding("help", gocui.KeyEsc, gocui.ModNone, closeHelp)
    }
    return nil
}

func closeHelp(g *gocui.Gui, v *gocui.View) error {
    if err := g.DeleteView("help"); err != nil {
        return err
    }
    g.DeleteKeybindings("help") // remove all temp bindings
    _, err := g.SetCurrentView("left")
    return err
}


func search(g *gocui.Gui, v *gocui.View) error {
    return nil
}
