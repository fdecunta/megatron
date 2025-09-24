package filetree

import (
    "errors"
    "fmt"
    "os"
    "path/filepath"
)

type Node struct {
    Parent *Node
    IsDir bool
    Size int64 
    Path string
    Children []*Node
}

func IsDir(path string) error {
    info, err := os.Stat(path)
    if err != nil {
        fmt.Println("Error:", err)
        return err
    }

    if !info.IsDir() {
        return errors.New("not a dir")
    }
    return nil
}

func BuildTree(root string, parent *Node) (*Node, error) {
    info, err := os.Stat(root)
    if err != nil {
        return nil, err
    }

    node := &Node {
        Parent: parent,
        IsDir: info.IsDir(),
        Size: info.Size(),
        Path: root,
    }

    if info.IsDir() { 
        files, err := os.ReadDir(root)
        if err != nil {
            return nil, err
        }

        for _, file := range files {
            childPath := filepath.Join(root, file.Name())
            childNode, err := BuildTree(childPath, node)
            if err != nil {
                return nil, err
            }
            node.Children = append(node.Children, childNode)
        }

        node.Size, err = dirSize(root)
    }

    return node, nil
}


func dirSize(path string) (int64, error) {
    var size int64
    err := filepath.Walk(path, func(_ string, info os.FileInfo, err error) error {
        if err != nil {
            return err
        }
        if !info.IsDir() {
            size += info.Size()
        }
        return nil
    })
    return size, err
}
