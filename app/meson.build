conf_data = configuration_data()
conf_data.set('APP_VERSION', '"@0@"'.format(meson.project_version()))
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
  'imageseqdialog.h'
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
  'imageseqdialog.cpp'
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

# TODO: macx, win32

app_pp = qt5.preprocess(
  qresources: app_qresources,
  moc_headers: app_headers,
  ui_files: app_ui_files
)

executable(
  'Pencil2D',
  app_src,
  app_pp,
  conf_file,
  gui_app: true,
  include_directories: core_lib_incdir,
  link_with: core_lib,
  dependencies: [qt5_dep, zlib_dep, quazip_dep],
  install: true
)
