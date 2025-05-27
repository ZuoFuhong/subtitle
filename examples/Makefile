UNAME_S := $(shell uname -s)
THIRD_PARTY_DIR := $(dir $(CURDIR:/=))third_party

ifeq ($(UNAME_S),Darwin)
	LDFLAGS = -L$(THIRD_PARTY_DIR)/onnxruntime/lib -L$(THIRD_PARTY_DIR)/fmt/lib
else
	LDFLAGS = -L$(THIRD_PARTY_DIR)/onnxruntime/lib64 -L$(THIRD_PARTY_DIR)/fmt/lib64
endif

CXX = clang++
CXXFLAGS = -std=c++20 -O3 -Wall -isystem $(THIRD_PARTY_DIR)/onnxruntime/include -isystem $(THIRD_PARTY_DIR)/fmt/include

SRCS = parakeet_tdt_asr.cc
TARGET = parakeet_tdt_asr

build: $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(LDFLAGS) -lonnxruntime -lfmt -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: build clean