# Simple Makefile for IoT socket library and examples 

CC = clang++
AR = ar
LD = clang++
CPPFLAGS = -std=c++17  -I./ -I/opt/iot/include -D__DEBUG__=1

LDFLAGS = -lpthread -lncurses -L/opt/iot/lib -liot

ROOTDIR = ./

CP = cp
ECHO = echo

BUILD_DIR = .

CPP_SOURCES_CLIENT = ./chat_client.cpp
CPP_SOURCES_SERVER = ./chat_server.cpp

CPP_HEADERS = 
C_SOURCES = 

APP = chat_client
SERVER = chat_server

OBJECTS_CLIENT = $(addprefix $(BUILD_DIR)/,$(notdir $(CPP_SOURCES_CLIENT:.cpp=.o)))
OBJECTS_SERVER = $(addprefix $(BUILD_DIR)/,$(notdir $(CPP_SOURCES_SERVER:.cpp=.o)))

vpath %.cpp $(sort $(dir $(CPP_SOURCES_CLIENT)))
vpath %.cpp $(sort $(dir $(CPP_SOURCES_SERVER)))
vpath %.cpp ./

# OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
# vpath %.c $(sort $(dir $(C_SOURCES)))

$(BUILD_DIR)/%.o: %.cpp $(CPP_HEADERS) Makefile | $(BUILD_DIR)
	$(ECHO) compiling $<
	$(CC) -c $(CPPFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(ECHO) compiling $<
	clang -c $(CFLAGS) $< -o $@

all: $(BUILD_DIR)/$(APP) $(BUILD_DIR)/$(SERVER)

$(BUILD_DIR)/$(APP): $(OBJECTS_CLIENT) Makefile
	$(ECHO) linking $<
	$(CC)  -o $@ $(OBJECTS_CLIENT) $(LDFLAGS)
	$(ECHO) successs

$(BUILD_DIR)/$(SERVER): $(OBJECTS_SERVER) Makefile
	$(ECHO) linking $<
	$(CC)  -o $@ $(OBJECTS_SERVER) $(LDFLAGS)
	$(ECHO) successs