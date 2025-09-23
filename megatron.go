package main

import (
    "fmt"
    "os"
    "log"
    "path/filepath"

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
    if len(os.Args) != 2 {
        usage()
        os.Exit(1)
    }

    root := os.Args[1]

    if err := filetree.IsDir(root); err != nil {
        log.Fatal(err)
    }

    rootNode, err := filetree.BuildTree(root, nil)
    if err != nil {
        log.Fatal(err)
    }

    if len(rootNode.Children) == 0 {
        fmt.Printf("Directory is empty")
        os.Exit(1)
    }

    currNode = rootNode
    selNode = rootNode.Children[0]


    /* ------------------------- */
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

//    if err := g.SetKeybinding("", gocui.KeyEnter, gocui.ModNone, openNode); err != nil {
//    	log.Panicln(err)
//    }

    currState = stack.State{0, 0}

    leftItems = leftItems[:0]
    for _, node := range currNode.Children {
        leftItems = append(leftItems, filepath.Base(node.Path))
    }

    rightItems = rightItems[:0]
    for _, node := range selNode.Children {
        rightItems = append(rightItems, filepath.Base(node.Path))
    }

    if err := g.MainLoop(); err != nil && err != gocui.ErrQuit {
        log.Panicln(err)
    }
    
}

func usage() {
    fmt.Println("Usage: megatron [DIR]")
    fmt.Println("  DIR  Path to filmoteca")
}


func layout(g *gocui.Gui) error {
    maxX, maxY := g.Size()

    t, err := g.SetView("title", 0, 0, maxX-1, 2)
    if err != nil && err != gocui.ErrUnknownView {
        return err
    }
    t.Clear()
    t.Frame = false
    fmt.Fprintf(t, "Megatron")


    l, err := g.SetView("left", 0, 3, maxX/2-1, maxY-1)
    if err != nil && err != gocui.ErrUnknownView {
        return err
    }
    l.Clear()
    l.Title = currNode.Path

    r, err := g.SetView("right", maxX/2, 3, maxX-1, maxY-1)
    if err != nil && err != gocui.ErrUnknownView {
        return err
    }
    r.Clear()
    if selNode == nil {
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

    if len(rightItems) == 0 {
        fmt.Fprintf(r, "--Empty--")
    } else {
        for _, rItem := range rightItems {
            fmt.Fprintf(r, "  %s\n", rItem)
        }    
    }

    return nil
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
    if len(currNode.Children) > 0 {
        writeRightItems()
    } else {
        rightItems = rightItems[:0]
        selNode = nil
    }
}


func openNode(g *gocui.Gui, l *gocui.View) error {
    if len(selNode.Children) == 0 {
        return nil
    }

    stateStack.Push(currState)
    currNode = selNode
    selNode = currNode.Children[0]

    currState.Selected = 0
    currState.Origin = 0

    writeLeftItems()
    writeRightItems()

    return nil
}


func closeNode(g *gocui.Gui, l *gocui.View) error {
    if currNode == rootNode {
        return nil
    }

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
        leftItems = append(leftItems, filepath.Base(node.Path))
    }
}


func writeRightItems() {
    rightItems = rightItems[:0]
    for _, node := range selNode.Children {
        rightItems = append(rightItems, filepath.Base(node.Path))
    }
}

func quit(g *gocui.Gui, l *gocui.View) error {
    return gocui.ErrQuit
}
