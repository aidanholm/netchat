CHECK  = no
DEBUG  = yes

# Directories
ODIR = obj
SDIR = src
IDIR = src
LDIR = lib
DDIR = deps

SHARED_SRCS = $(shell find $(SDIR)/{,utilities} -maxdepth 1 -type f -name '*.c')
SERVER_SRCS = $(shell find $(SDIR)/server -type f -name '*.c') $(SHARED_SRCS)
CLIENT_SRCS = $(shell find $(SDIR)/client -type f -name '*.c') $(SHARED_SRCS)

OBJS = $(patsubst $(SDIR)/%,$(ODIR)/%,$(1:.c=.o))

#
# Compiler options
#

CC = clang

CFLAGS  = -std=gnu99 -x c \
		  -Wall -Wextra -Wformat=2 -Wfloat-equal -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Wswitch-default -Wno-format-invalid-specifier\
		  -fdata-sections -ffunction-sections \
          -I$(IDIR) -I$(IDIR)/utilities \
		  `pkg-config --cflags ncursesw` \
		  #-fsanitize=thread \

LIBS    = -lpthread \
		  `pkg-config --libs ncursesw` \

LDFLAGS = --gc-sections -O1

# Debugging option
ifeq ($(DEBUG),yes)
	CFLAGS  += -g -g3 -ggdb -fstack-protector-all -fstack-protector-strong \
			   -Wl,-z,relro -Wl,-z,now -Wstack-protector
else
	CFLAGS  += -O3 -DNDEBUG -s
endif

# Extra arguments for clang
ifeq ($(CC),clang)
	CFLAGS += -Qunused-arguments -fcolor-diagnostics
endif

# Use scan-build if checking is enabled
ifeq ($(CHECK),yes)
	CC:=scan-build -o ./scan $(CC)
endif

# Recipes

COL_GREEN   =\\x1b[32m
COL_BLUE    =\\x1b[34m
COL_RST     =\\x1b[0m

MSG_COMP =$(COL_GREEN)"Compiling" $(COL_RST)
MSG_LINK =$(COL_BLUE)"Linking  " $(COL_RST)

all: server client tags

# Build object file from source file
$(ODIR)/%.o: $(SDIR)/%.c Makefile
	@# Ensure that the required directories exist...
	@mkdir -p "$(@D)"
	@mkdir -p "$(patsubst $(ODIR)%,$(DDIR)%,$(@D))"

	@# Compile source file, and generate dependency file
	@echo -e $(MSG_COMP) "$<" $(DONE)
	@ccache $(CC) -MMD -c -o $@ $< $(CFLAGS)

	@cp $(@:.o=.d) $(DDIR)/$*.P; \
	 sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
		 -e '/^$$/ d' -e 's/$$/ :/' < $(@:.o=.d) >> $(DDIR)/$*.P; \
	 rm -f $(@:.o=.d)
-include $(shell find -L $(DDIR) -type f -name '*.P')

# Build target from object files
comma:= ,
empty:=
space:= $(empty) $(empty)

server: $(call OBJS,$(SERVER_SRCS))
	@echo -e $(MSG_LINK) "$@"
	@ccache $(CC) -o $@ $^ $(CFLAGS) -Wl,$(subst $(space),$(comma),$(LDFLAGS)) $(LIBS)

client: $(call OBJS,$(CLIENT_SRCS))
	@echo -e $(MSG_LINK) "$@"
	@ccache $(CC) -o $@ $^ $(CFLAGS) -Wl,$(subst $(space),$(comma),$(LDFLAGS)) $(LIBS)

tags: $(SERVER_SRCS) $(CLIENT_SRCS)
	@echo "[Tags] Making tags exuberantly"
	@ctags -R src > $@

.PHONY: clean

clean:
	@echo "Cleaning files..."
	@rm -rf $(call OBJS,$(SERVER_SRCS)) $(call OBJS,$(CLIENT_SRCS)) *~ $(INCDIR)/*~ $(DDIR)/*.P scan/*
