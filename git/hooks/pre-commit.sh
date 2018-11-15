#!/bin/bash
#-------------------------------------------------------------------------------
# File: pre-commit.sh
# Author: Elvin-Alin Sindrilaru <esindril@cern.ch>
#-------------------------------------------------------------------------------
#
#/************************************************************************
# * EOS - the CERN Disk Storage System                                   *
# * Copyright (C) 2016 CERN/Switzerland                                  *
# *                                                                      *
# * This program is free software: you can redistribute it and/or modify *
# * it under the terms of the GNU General Public License as published by *
# * the Free Software Foundation, either version 3 of the License, or    *
# * (at your option) any later version.                                  *
# *                                                                      *
# * This program is distributed in the hope that it will be useful,      *
# * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
# * GNU General Public License for more details.                         *
# *                                                                      *
# * You should have received a copy of the GNU General Public License    *
# * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
# ************************************************************************/

#-------------------------------------------------------------------------------
# Description: Pre-commit script which is called automatically when executing
# the git commit command. The commit will fail if the script returns a non-zero
# value. It applies Astyle formatting to all files added in the current commit.
#-------------------------------------------------------------------------------

# Check for astyle executable
ASTYLE=$(which astyle)

if [ $? -ne 0 ]; then
  echo "[!] Astyle is not installed. Download and install it from here:" >&2
  echo "https://sourceforge.net/projects/astyle/files/" >&2
  exit 1
fi

# Set the astyle options to be used
GIT_ROOT=$(git rev-parse --show-toplevel)
ARTISTIC_STYLE_OPTIONS=$(cat ${GIT_ROOT}/utils/astylerc | grep -v '^#' | tr '\n' ' ')
#echo "Astyle options: ${ARTISTIC_STYLE_OPTIONS}"

# Grab all the files to be committed i.e. the ones added to the local index
FILES=$(git diff --cached --name-only --diff-filter=ACMR | grep -E "\.(c|h|cpp|hpp|cc|hh)$")
set -e

for FILE in ${FILES}; do
    ${ASTYLE} --options=none ${ARTISTIC_STYLE_OPTIONS} -n ${FILE}
    git add ${FILE}
done

exit 0
