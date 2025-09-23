BIN_DIR := /usr/local/bin
CONFIG_DIR := $(HOME)/.config/megatron

install: megatron
	mkdir -p $(CONFIG_DIR)
	touch $(CONFIG_DIR)/config.txt
	sudo cp megatron $(BIN_DIR)

remove:
	rm -rf $(CONFIG_DIR)
	sudo rm $(BIN_DIR)/megatron

megatron: megatron.go 
	go build megatron.go

run:
	mkdir -p $(CONFIG_DIR)
	touch $(CONFIG_DIR)/config.txt
	go run megatron.go ~/Filmoteca

.PHONY: install remove run
