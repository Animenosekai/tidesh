#!/usr/bin/env tidesh
# ==============================================================================
# 🌊 tidesh Comprehensive Feature Showcase (example.sh)
# ==============================================================================
# This script demonstrates all major features, built-in commands, expansions,
# context managers, exception handling, and dual syntax styles (POSIX vs. tidesh block-style).
#
# Run this script with:
#   tidesh example.sh
#   # or directly from your build directory:
#   # ./bin/tidesh-* example.sh
# ==============================================================================

echo "================================================================================"
echo "🌊 Welcome to the comprehensive tidesh feature showcase!"
echo "================================================================================"

# ==============================================================================
# 1. BUILT-IN ENVIRONMENT & SHELL VARIABLES
# ==============================================================================
echo ""
echo "--- 1. tidesh Environment Variables ---"
echo "Shell Name:       $TIDESH_NAME"
echo "Shell Version:    $TIDESH_VERSION (Raw: $TIDESH_RAW_VERSION, Git: $TIDESH_GIT_VERSION)"
echo "Build Date:       $TIDESH_BUILD_DATE"
echo "Platform:         $TIDESH_PLATFORM"
echo "Compiler:         $TIDESH_COMPILER"
echo "Build Type:       $TIDESH_BUILD_TYPE"
echo "Process ID:       $TIDESH_PID (Parent: $TIDESH_PPID)"
echo "Executable:       $TIDESH_EXECUTABLE"
echo "Current Shell:    $SHELL (Level: $SHLVL)"

# ==============================================================================
# 2. VARIABLE & EXPANSION FEATURES
# ==============================================================================
echo ""
echo "--- 2. Variable & Parameter Expansions ---"

# Basic variable assignment and expansion
MSG="Hello, tidesh"
echo "Basic expansion:  $MSG or ${MSG}"

# Default value modifiers
unset UNSET_VAR
EMPTY_VAR=""
echo "Default if unset: ${UNSET_VAR:-default_val}"
echo "Assign default:   ${ASSIGN_VAR:=assigned_val} (now: $ASSIGN_VAR)"
echo "Alternative val:  ${MSG:+alternative_active}"

# String length
echo "String length:    \${#MSG} = ${#MSG}"

# Whitespace splitting with ${=VAR}
FRUITS="apple banana cherry"
echo "Without splitting word count test:"
for f in "$FRUITS"; do
    echo "  Single token: $f"
done
echo "With whitespace splitting (\${=FRUITS}):"
for f in ${=FRUITS}; do
    echo "  Split token:  $f"
done

# Arithmetic expansion $(( ... ))
NUM=$(( (10 + 5) * 2 - 4 / 2 ))
echo "Arithmetic:       \$(( (10 + 5) * 2 - 4 / 2 )) = $NUM"

# Tilde Expansions
echo "Tilde home:       ~"
echo "Tilde current:    ~+ (PWD)"
echo "Tilde previous:   ~- (OLDPWD)"

# Brace Expansions
echo -n "Comma braces:     "
echo a{b,c,d}e
echo -n "Numeric range:    "
echo {1..5}
echo -n "Padded range:     "
echo file_{01..04}.txt
echo -n "Character range:  "
echo {a..e}
echo -n "Reverse range:    "
echo {5..1}
echo -n "Nested braces:    "
echo a{{,c}d,e}

# Command & Process Substitution
CURRENT_DATE=$(date +%Y-%m-%d 2>/dev/null || echo "2026-10-26")
echo "Command substitution \$(...): Current date is $CURRENT_DATE"

# Filename Globbing (*, **, ?, [...])
echo -n "Recursive globbing (**/*.h count): "
find include -name "*.h" | wc -l | tr -d ' '
echo "  Glob pattern include/**/*.h matched headers recursively"

# ==============================================================================
# 3. CONTROL FLOW: CONDITIONALS (POSIX vs. BLOCK STYLE)
# ==============================================================================
echo ""
echo "--- 3. Conditional Statements (POSIX vs. tidesh Block Style) ---"

TEST_VAL=10

# POSIX Style: if ...; then ...; elif ...; then ...; else ...; fi
echo "[POSIX style if/then/elif/else/fi]"
if [ "$TEST_VAL" -gt 20 ]; then
    echo "  POSIX: Greater than 20"
elif [ "$TEST_VAL" -eq 10 ]; then
    echo "  POSIX: Exact match (10)"
else
    echo "  POSIX: Less than 10"
fi

# tidesh Block Style (C-style): if condition { ... } else if ... { ... } else { ... }
echo "[tidesh Block style if { ... } else if { ... } else { ... }]"
if [ "$TEST_VAL" -gt 20 ] {
    echo "  Block: Greater than 20"
} else if [ "$TEST_VAL" -eq 10 ] {
    echo "  Block: Exact match (10)"
} else {
    echo "  Block: Less than 10"
}

# tidesh Parenthesized Condition syntax: if (condition) { ... }
if (test "$TEST_VAL" -eq 10) {
    echo "  Block with parentheses: if (test ...) { ... } works!"
}

# Negation builtin 'not'
if not false {
    echo "  'not' builtin: 'not false' evaluates successfully"
}

# ==============================================================================
# 4. CONTROL FLOW: FOR LOOPS (POSIX vs. BLOCK STYLE)
# ==============================================================================
echo ""
echo "--- 4. For Loops (POSIX vs. tidesh Block Style) ---"

# POSIX Style: for var in words; do ...; done
echo -n "POSIX for loop:  "
for item in alpha beta gamma; do
    echo -n "$item "
done
echo ""

# tidesh Block Style: for var in words { ... }
echo -n "Block for loop:  "
for item in 1 2 3 4 5 {
    if [ "$item" -eq 3 ] {
        continue
    }
    if [ "$item" -eq 5 ] {
        break
    }
    echo -n "$item "
}
echo "(skipped 3, broke at 5)"

# Parenthesized for loop condition: for (x in list) { ... }
echo -n "Paren for loop:  "
for (color in red green blue) {
    echo -n "$color "
}
echo ""

# ==============================================================================
# 5. CONTROL FLOW: WHILE & UNTIL LOOPS (POSIX vs. BLOCK STYLE)
# ==============================================================================
echo ""
echo "--- 5. While and Until Loops (POSIX vs. tidesh Block Style) ---"

# POSIX while loop
COUNT=3
echo -n "POSIX while:     "
while [ "$COUNT" -gt 0 ]; do
    echo -n "$COUNT "
    COUNT=$(( COUNT - 1 ))
done
echo ""

# tidesh Block Style while loop: while condition { ... }
COUNT=0
echo -n "Block while:     "
while [ "$COUNT" -lt 3 ] {
    COUNT=$(( COUNT + 1 ))
    echo -n "$COUNT "
}
echo ""

# Block Style until loop: until condition { ... }
UNTIL_COUNT=0
echo -n "Block until:     "
until [ "$UNTIL_COUNT" -ge 3 ] {
    UNTIL_COUNT=$(( UNTIL_COUNT + 1 ))
    echo -n "$UNTIL_COUNT "
}
echo ""

# Parenthesized while loop: while (condition) { ... }
P_VAL=2
echo -n "Paren while:     "
while (test "$P_VAL" -gt 0) {
    echo -n "$P_VAL "
    P_VAL=$(( P_VAL - 1 ))
}
echo ""

# ==============================================================================
# 6. CONTROL FLOW: CASE STATEMENTS
# ==============================================================================
echo ""
echo "--- 6. Case Statements (Pattern Matching & Fallthroughs) ---"

TARGET="test.tar.gz"

# Standard case with alternative patterns '|' and wildcard
case "$TARGET" in
    *.txt)
        echo "  Regular text file"
        ;;
    *.tar.gz|*.tgz)
        echo "  Compressed tarball archive: $TARGET"
        ;;
    *)
        echo "  Unknown file type"
        ;;
esac

# Advanced Clause Terminators: ;& (fallthrough) and ;;& (continue testing)
ACTION="step_one"
echo "Demonstrating ;& (fallthrough to next clause):"
case "$ACTION" in
    step_one)
        echo "  Clause 1: executing step_one"
        ;&
    step_two)
        echo "  Clause 2: fell through to step_two without pattern check!"
        ;;
    step_three)
        echo "  Clause 3: step_three"
        ;;
esac

FILE="image.png"
echo "Demonstrating ;;& (continue testing subsequent patterns):"
case "$FILE" in
    *.png)
        echo "  Matched PNG format"
        ;;&
    image.*)
        echo "  Also matched prefix 'image.'"
        ;;
esac

# ==============================================================================
# 7. EXCEPTION HANDLING: TRY / CATCH / FINALLY
# ==============================================================================
echo ""
echo "--- 7. Exception Handling (try / catch / finally) ---"

# Basic try/catch/finally
try {
    echo "  Entering try block: will trigger an error (exit 1)..."
    (exit 1)
    echo "  Unreachable line in try block"
} catch {
    echo "  Caught error successfully! Exit code was: $TIDE_TRY_STATUS ($?)"
} finally {
    echo "  Finally block always executes!"
}

# Targeted catch by exit code: catch <code> or catch (<c1>, <c2>)
try {
    echo "  Testing targeted catch block with exit code 42..."
    (exit 42)
} catch 1 {
    echo "  Caught code 1"
} catch (40, 41, 42) {
    echo "  Caught code in list (40, 41, 42)! Caught code: $TIDE_TRY_STATUS"
} catch {
    echo "  Fallback catch-all"
}

# ==============================================================================
# 8. FUNCTIONS: POSIX & ADVANCED TIDESH NAMED ARGUMENTS
# ==============================================================================
echo ""
echo "--- 8. Shell Functions ---"

# POSIX standard function
posix_func() {
    echo "  POSIX function called with \$1='$1' and \$2='$2'"
}
posix_func "alpha" "beta"

# tidesh Advanced Named Parameters & Keyword Arguments syntax:
# Definition: func_name(param1, param2) { ... }
# Call:       func_name(param1="val", param2="val")
print_profile(username, role, language) {
    echo "  User Profile: $username | Role: $role | Language: $language"
}

# Calling with positional arguments
print_profile "Alice" "Developer" "C"

# Calling with named/keyword arguments!
print_profile(language="tidesh", username="Bob", role="Architect")

# ==============================================================================
# 9. CONTEXT MANAGERS: THE 'with' STATEMENT
# ==============================================================================
echo ""
echo "--- 9. tidesh Context Managers ('with' blocks) ---"

# 9.1 cwd(path) - Change directory temporarily and guarantee return
ORIGINAL_DIR=$(pwd)
with cwd("/tmp") {
    echo "  Inside cwd(\"/tmp\"): $(pwd)"
}
echo "  Outside cwd:          $(pwd) (Restored: $([ "$ORIGINAL_DIR" = "$(pwd)" ] && echo "yes"))"

# 9.2 env(KEY=VAL, ...) - Temporarily set/overwrite environment variables
export APP_STAGE="production"
with env(APP_STAGE="testing", MOCK_DB="1", SECRET_KEY="temp123") {
    echo "  Inside env(): APP_STAGE=$APP_STAGE, MOCK_DB=$MOCK_DB, SECRET_KEY=$SECRET_KEY"
}
echo "  Outside env(): APP_STAGE=$APP_STAGE (MOCK_DB is '${MOCK_DB:-unset}')"

# 9.3 do ... done syntax for context managers (alternative to { ... })
with env(SYNTAX_STYLE="do_done") do
    echo "  'with ... do ... done' syntax active: SYNTAX_STYLE=$SYNTAX_STYLE"
done

# 9.4 options(...) - Temporarily alter shell options (e.g. -e for errexit, +x)
with options("-e") {
    echo "  Inside options('-e'): errexit active for this block"
}

# 9.5 umask(...) - Temporarily change file creation mask
with umask("077") {
    echo "  Inside umask('077'): file creation restricted to user only"
}

# 9.6 tempdir(var=...) - Create isolated temporary directory & automatically clean up
with tempdir(var="WORK_DIR") {
    echo "  Inside tempdir: created and switched into $WORK_DIR"
    echo "temporary data" > temp_file.txt
    echo "  Created file: $(ls temp_file.txt)"
}
echo "  Outside tempdir: temporary workspace cleaned up automatically"

# 9.7 timeout(...) - Enforce execution time limit
with timeout("2s") {
    echo "  Inside timeout('2s'): running command safely within timeout limit"
    sleep 0.1
}

# 9.8 session - Snapshot and revert all session state mutations
with session {
    echo "  Inside session snapshot: setting temporary alias, export, and function..."
    alias temp_alias="echo 'temporary alias'"
    export TMP_SESSION_VAR="active"
    session_scoped_func() { echo "I will disappear after the block"; }
    session_scoped_func
}
# Everything inside reverted!
echo "  Outside session: TMP_SESSION_VAR is '${TMP_SESSION_VAR:-reverted}'"

# 9.9 allowed_exit_codes(...) - Allow specific exit codes inside try blocks
with allowed_exit_codes(1, 2) {
    try {
        echo "  Inside allowed_exit_codes(1, 2): (exit 1) will NOT trigger catch..."
        (exit 1)
        echo "  (exit 1) was allowed and did not halt execution!"
    } catch {
        echo "  Unexpected catch"
    }
}

# 9.10 limits(...) & procs(...) - Resource limits & Fork-bomb protection
with procs(20) {
    echo "  Inside procs(20): process fork limit active"
}

with limits(memory="1GB", cpu="10s") {
    echo "  Inside limits(memory='1GB', cpu='10s'): resource caps active"
}

# 9.11 sandbox(...) - Unified sandbox combining filesystem & resource restrictions
with sandbox(readonly=true, timeout="5s", procs=10) {
    echo "  Inside unified sandbox(readonly=true, timeout='5s', procs=10)"
}

# 9.12 Custom Context Managers with 'context-manager' builtin
# Custom context managers receive state tokens from --enter via $1 and status via $2
echo "[Custom Context Manager]"
context-manager register tracker \
    --enter 'echo "custom_token_123"' \
    --exit 'echo "  [tracker exit hook] State token: $1 | Exit status of body: $2"'

with tracker("arg1") {
    echo "  Inside custom tracker() context manager body"
}
context-manager unregister tracker

# ==============================================================================
# 10. BUILT-IN COMMANDS HIGHLIGHTS
# ==============================================================================
echo ""
echo "--- 10. Built-in Commands Highlights ---"

# Aliases
alias greet_alias="echo '  Hello from greet_alias!'"
eval 'greet_alias'
unalias greet_alias

# Directory Stack (pushd, popd)
pushd /tmp > /dev/null
echo "  After pushd /tmp: current directory is $(pwd)"
popd > /dev/null
echo "  After popd:       returned to $(pwd)"

# Features builtin
echo "  features status context_managers: $(features status context_managers 2>/dev/null | tr '\n' ' ')"

# Hooks builtin
echo "  hooks status:                     $(hooks status 2>/dev/null | tr '\n' ' ')"

# Type & Command builtins
echo -n "  type builtin check: "
type cd
type echo

# ==============================================================================
# 11. PIPELINES AND REDIRECTIONS ON COMPOUND STATEMENTS
# ==============================================================================
echo ""
echo "--- 11. Pipelines & Redirections on Compound Statements ---"

# Pipeline directly from a for loop block
echo -n "  For loop piped to grep: "
for val in kiwi orange strawberry banana blueberry {
    echo "$val"
} | grep "berry" | tr '\n' ' '
echo ""

# Redirection on a block-style if statement
if true {
    echo "Line 1 from redirected block"
    echo "Line 2 from redirected block"
} > /tmp/tidesh_example_block_redir.txt

echo "  Read back from redirected if block file:"
while read -r line {
    echo "    $line"
} < /tmp/tidesh_example_block_redir.txt
rm -f /tmp/tidesh_example_block_redir.txt

echo ""
echo "================================================================================"
echo "✅ tidesh example.sh showcase finished successfully!"
echo "================================================================================"
