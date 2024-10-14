#!/bin/bash

if [[ -z "${IDF_PATH}" ]]; then
  echo "IDF_PATH is not set, cannot find path to esp-idf. Please set it (using get_idf) and re-run this script."
  exit -1
else
  echo "Applying patches to esp-idf in '${IDF_PATH}'"
fi

cur_dir=$(pwd)
patches=($(find patches -type f))
cd "${IDF_PATH}"
for patch in "${patches[@]}"; do
    echo "Applying patch: ${patch}"
    git apply ${cur_dir}/${patch}
done

cd ${cur_dir}
