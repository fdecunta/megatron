package stack

type State struct {
    Selected int
    Origin int
}


type Stack struct {
    Items []State
}


func (s *Stack) Push(n State) {
    s.Items = append(s.Items, n)
}


func (s *Stack) Pop() {
    if len(s.Items) == 0 {
        return 
    } 
    s.Items = s.Items[:len(s.Items)-1]
}

func (s *Stack) IsEmpty() bool {
    if len(s.Items) == 0 {
        return true
    } else {
        return false
    }
}

func (s *Stack) Top() State {
    return s.Items[len(s.Items)-1]
}
