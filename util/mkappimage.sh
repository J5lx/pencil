#!/bin/bash

status() {
  printf "\e[1m\e[32m==>\e[39m $1\e[0m\n"
}

error() {
  printf "\e[1m\e[31m==> ERROR:\e[39m $1\e[0m\n"
}

fail() {
  error "A failure occured while making the AppImage."
  cleanup
  exit -1
}

find_program() {
  for hint in "$@"; do
    if [ -z "${hint}" ]; then
      continue;
    fi
    local ret="$(PATH=".:${PATH}" which "${hint}")"
    if [ $? -eq 0 ]; then
      readlink -e "${ret}"
      break
    fi
  done
}

setup() {
  trap fail ERR

  builddir="$(readlink -e "$1")"
  version="$2"
  outfile="$(readlink -f "$3")"

  linuxdeployqt="$(find_program "$4" "linuxdeployqt"{{"-continuous-x86_64",}".AppImage",})"
  if [ ! -x "${linuxdeployqt}" ]; then
    error "Unable to find linuxdeployqt"
    fail
  fi

  ffmpeg="$(find_program "$5" "ffmpeg")"
  if [ ! -x "${ffmpeg}" ]; then
    error "Unable to find FFmpeg"
    fail
  fi

  workdir="$(mktemp -d)"

  pushd "${workdir}" &>/dev/null
}

cleanup() {
  trap - ERR
  if [ -d "${workdir}" ]; then
    popd &>/dev/null
    rm -rf "${workdir}"
  fi
}

if [ $# -lt 3 ]; then
  echo "Usage: $0 BUILD-DIRECTORY VERSION OUTPUT-FILE [LINUXDEPLOYQT] [FFMPEG]"
  exit -1
fi

setup "$@"

status "Making AppImage: $(basename "${outfile}")"
status "Using linuxdeployqt from ${linuxdeployqt}"
status "Using FFmpeg from ${ffmpeg}"

status "Installing Pencil2D..."
make -C "${builddir}" install INSTALL_ROOT="${workdir}/installroot"

prefix="$(cd "${workdir}/installroot"; find * -path '*bin/pencil2d' -printf '/%h' -quit | xargs dirname | sed 's#^/$##')"
if [ "${prefix}" != "/usr" ]; then
  error "Please reconfigure Pencil2D with PREFIX=/usr (detected: PREFIX=${prefix})"
  fail
fi

status "Patching desktop entry..."
sed -i "/^\[Desktop Entry\]$/aX-AppImage-Version=${version}
        /^Keywords\(\[[a-zA-Z_.@]\+\]\)\?=/d
        /^Version=/cVersion=1.0" \
  "${workdir}/installroot/usr/share/applications/pencil2d.desktop"

status "Installing FFmpeg..."
install -Dm755 "${ffmpeg}" "${workdir}/installroot/usr/plugins/ffmpeg"

status "Creating AppImage..."
"${linuxdeployqt}" \
  "${workdir}/installroot/usr/share/applications/pencil2d.desktop" \
  -appimage \
  -executable="${workdir}/installroot/usr/plugins/ffmpeg" \
  -extra-plugins=iconengines/libqsvgicon.so

mv -Tf "${workdir}/Pencil2D-${version// /_}-"*".AppImage" "${outfile}"

cleanup

status "Finished making: $(basename "${outfile}")"
