#!/bin/bash
cd "$(dirname "$0")"
if [ -n "$CONDA_PREFIX" ]; then
    echo "Note: conda активна ($CONDA_PREFIX), деактивирую"
    export PATH="$(echo "$PATH" | tr ':' '\n' | grep -v conda | paste -sd:)"
    unset CONDA_PREFIX CONDA_DEFAULT_ENV CONDA_SHLVL
fi
export LD_LIBRARY_PATH="$HOME/sdkreverse/sdk_libs:$LD_LIBRARY_PATH"
export QT_QPA_PLATFORM=xcb
export QT_LOGGING_RULES='*.debug=false;qt.qpa.*=false'
exec ./apps/demo "$@"
