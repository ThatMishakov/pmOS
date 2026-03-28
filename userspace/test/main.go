package main

import (
	"fmt"
	"strconv"
)

func main() {
	for i := 0; i < 10; i++ {
		go func(i int) {
			fmt.Println("!!! Go: Hello from iteration " + strconv.Itoa(i))
		}(i)
	}
}
