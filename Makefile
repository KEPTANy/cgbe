NAME        = cgbe

BIN_DIR     = bin
BUILD_DIR   = build
INCLUDE_DIR = include
SRC_DIR     = src
TEST_DIR    = test

SRC_MAIN    = $(NAME).c
OBJ_MAIN    = $(SRC_MAIN:%.c=$(BUILD_DIR)/%.o)

SRCS        = $(shell find $(SRC_DIR) -name '*.c')
OBJS        = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS        = $(OBJS:.o=.d)

TARGET      = $(BIN_DIR)/$(NAME)

CC          = clang
CFLAGS      = -O2 -Wall -Wextra -std=c23 -pedantic-errors
CPPFLAGS    = -MMD -MP -I$(INCLUDE_DIR)/

RM          = rm -f

TESTS       = $(shell find $(TEST_DIR) -name '*.c')
TEST_OBJS   = $(TESTS:%.c=$(BUILD_DIR)/%.o)
TEST_BINS   = $(TESTS:%.c=$(BIN_DIR)/%)

.PHONY: all clean fclean re check-style
.PRECIOUS: $(BIN_DIR)/% $(BUILD_DIR)/%.o $(BUILD_DIR)/%.d
.DEFAULT_GOAL: all

$(BUILD_DIR)/%.o: %.c
	@echo ! Started building $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@
	@echo ! Finished building $@

$(BIN_DIR)/%: $(OBJS) $(BUILD_DIR)/%.o
	@echo ! Started linking $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@
	@echo ! Finished linking $@

all: $(TARGET)

clean:
	@$(RM) -r $(BUILD_DIR)
	@echo ! Build directory deleted

fclean: clean
	@$(RM) -r $(BIN_DIR)
	@echo ! Bin directory deleted

test: $(TEST_BINS)
	@echo ! Running tests
	@for test in $(TEST_BINS); do \
		echo ----- $$test -----; \
		./$$test; \
	done

re:
	@echo ! Started rebuilding everything
	@$(MAKE) fclean
	@$(MAKE) all
	@echo ! Finished rebuilding everything

check-style:
	@clang-format --dry-run --Werror $(shell find \
		$(SRC_DIR) $(INCLUDE_DIR) $(TEST_DIR) -name '*.c' -o -name '*.h')
	@echo ! No style violations
