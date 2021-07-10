ifndef PORT
PORT=/dev/ttyUSB0
endif
BOARD=esp8266:esp8266:nodemcuv2

SRC = $(wildcard *.ino)
PROJECT = $(notdir $(CURDIR))

TARGET = $(PROJECT).ino.bin

$(TARGET): $(SRC)
	@rm -rf tmp
	@mkdir -p tmp
	@TMPDIR=$(PWD)/tmp arduino-cli compile --fqbn=$(BOARD) --output-dir=$(PWD)
	@rm -rf tmp

recompile: $(TARGET)

netupload: $(TARGET)
ifdef host
	curl -F "image=@$(TARGET)" ${host}:8266/update
else
	@echo Need host=target to be set - eg make $@ host=testesp
endif

upload:
	@mkdir -p tmp
	@TMPDIR=$(PWD)/tmp arduino-cli upload --fqbn=$(BOARD) -p $(PORT) --input-dir=$(PWD)
	@rm -rf tmp

serial:
	@kermit -l $(PORT) -b 115200 -c

clean:
	rm -rf *.elf tmp *.bin
