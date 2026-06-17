#!/bin/bash
# SessionStart hook for Quiet Rift: Enigma
# Surfaces "where we left off" so a fresh session doesn't need the user
# to re-explain the project state. Reads CLAUDE.md still lives — this
# adds the volatile bits (branch, recent commits, status doc mtimes).
set -euo pipefail

# Only run in remote/web sessions — local terminal users don't need this.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-.}"

# Bail silently if not a git repo.
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || exit 0

# The long-running dev branch all work lands on.
DEV_BRANCH="claude/unreal-cpp-blueprint-project-0F07i"

echo "## Session start snapshot — $(date -u '+%Y-%m-%d %H:%M UTC')"
echo
echo "### Branch"
echo "Currently on: \`$(git branch --show-current 2>/dev/null || echo unknown)\`"
echo
echo "Dev branch convention: \`${DEV_BRANCH}\` (per CLAUDE.md)."
echo "If the harness put this session on a different \`claude/*\` branch,"
echo "switch back unless the user says otherwise."
echo
echo "### Pull latest from git (copy-paste these two lines)"
echo '```'
echo "git checkout ${DEV_BRANCH}"
echo "git pull origin ${DEV_BRANCH}"
echo '```'
echo "If git complains about local changes, run \`git stash\` first, then"
echo "pull, then \`git stash pop\` afterward — or ask Claude to walk you through it."
echo
echo "### Last 10 commits on current branch"
echo '```'
git log --oneline -10 2>/dev/null || echo "(no commits)"
echo '```'
echo
echo "### Working tree"
if [ -z "$(git status --porcelain 2>/dev/null)" ]; then
  echo "Clean."
else
  echo '```'
  git status --short
  echo '```'
fi
echo
echo "### Ahead/behind origin/main"
if git rev-parse origin/main >/dev/null 2>&1; then
  AHEAD=$(git rev-list --count origin/main..HEAD 2>/dev/null || echo "?")
  BEHIND=$(git rev-list --count HEAD..origin/main 2>/dev/null || echo "?")
  echo "$AHEAD ahead, $BEHIND behind \`origin/main\`"
else
  echo "(origin/main not fetched)"
fi
echo
echo "### Status docs — most recently updated first"
echo "The doc with the newest commit timestamp is where active work was happening."
echo "Read it after CLAUDE.md to catch up."
echo
echo '```'
{
  for f in CLAUDE.md \
           SYSTEM_COHESION_AUDIT.md \
           GDD_IMPLEMENTATION_STATUS.md \
           MANUAL_EDITOR_TASKS.md \
           PROCEDURAL_WORLD_PLAN.md \
           QuietRiftEnigma/GAME_OVERVIEW.md; do
    if [ -f "$f" ]; then
      TS=$(git log -1 --format='%ai' -- "$f" 2>/dev/null)
      if [ -z "$TS" ]; then
        TS="(uncommitted)         "
      fi
      printf '%s  %s\n' "$TS" "$f"
    fi
  done
} | sort -r
echo '```'
