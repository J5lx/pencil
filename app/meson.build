git = find_program('git', required: false)
git_current_sha1 = ''
git_timestamp = ''
if git.found()
  git_current_sha1 = run_command(git, [
    '--git-dir=.git',
    '--work-tree=.',
    '-C',
    meson.source_root(),
    'rev-parse',
    'HEAD'
  ]).stdout().strip()
  git_timestamp = run_command(git, [
    '--git-dir=.git',
    '--work-tree=.',
    '-C',
    meson.source_root(),
    'log',
    '-1',
    '--format=%cd',
    '--date=local'
  ]).stdout().strip()
endif

conf_data = configuration_data()
conf_data.set('APP_VERSION', '"@0@"'.format(meson.project_version()))
conf_data.set('GIT_EXISTS', git.found())
conf_data.set('GIT_CURRENT_SHA1', '"@0@"'.format(git_current_sha1))
conf_data.set('GIT_TIMESTAMP', '"@0@"'.format(git_timestamp))
conf_file = configure_file(output: 'config.h', configuration: conf_data)

app_qresources = [
  'resource/app.qrc',
  '../translations/translations.qrc',
  '../pencil.qrc'
]

app_headers = [
  'mainwindow2.h',
  'shortcutfilter.h',
  'exportimageseqdialog.h',
  'timeline2.h',
  'actioncommands.h',
  'preferencesdialog.h',
  'shortcutspage.h',
  'preview.h',
  'colorbox.h',
  'colorgrid.h',
  'colorgriditem.h',
  'colorinspector.h',
  'colorpalettewidget.h',
  'colorwheel.h',
  'filedialogex.h',
  'displayoptionwidget.h',
  'pencilapplication.h',
  'exportmoviedialog.h',
  'app_util.h',
  'errordialog.h',
  'imageseqdialog.h',
  'aboutdialog.h'
]

app_src = [
  'main.cpp',
  'mainwindow2.cpp',
  'shortcutfilter.cpp',
  'exportimageseqdialog.cpp',
  'timeline2.cpp',
  'actioncommands.cpp',
  'preferencesdialog.cpp',
  'shortcutspage.cpp',
  'preview.cpp',
  'colorbox.cpp',
  'colorgrid.cpp',
  'colorgriditem.cpp',
  'colorinspector.cpp',
  'colorpalettewidget.cpp',
  'colorwheel.cpp',
  'filedialogex.cpp',
  'displayoptionwidget.cpp',
  'pencilapplication.cpp',
  'exportmoviedialog.cpp',
  'errordialog.cpp',
  'imageseqdialog.cpp',
  'aboutdialog.cpp'
]

app_ui_files = [
  'ui/mainwindow2.ui',
  'ui/exportimageseqdialog.ui',
  'ui/timeline2.ui',
  'ui/shortcutspage.ui',
  'ui/colorinspector.ui',
  'ui/colorpalette.ui',
  'ui/displayoption.ui',
  'ui/exportmoviedialog.ui',
  'ui/errordialog.ui'
]

app_rc = []
if host_machine.system() == 'darwin'
  # TODO: ../pencil.icns (Couldn’t quite figure that out myself (yet))
  install_data('../Info.plist', install_dir: 'Contents')
  install_data(
    '../icons/mac_pcl_icon.icns',
    '../icons/mac_pclx_icon.icns',
    install_dir: 'Contents/Resources'
  )
endif
if host_machine.system() == 'windows'
  windows = import('windows')
  app_rc = windows.compile_resources('../pencil.rc')
endif

app_pp = qt5.preprocess(
  qresources: app_qresources,
  moc_headers: app_headers,
  ui_files: app_ui_files
)

executable(
  'Pencil2D',
  app_src,
  app_rc,
  app_pp,
  conf_file,
  gui_app: true,
  include_directories: core_lib_incdir,
  link_with: core_lib,
  dependencies: [qt5_dep, zlib_dep, quazip_dep],
  install: true
)
