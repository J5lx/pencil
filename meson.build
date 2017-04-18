project('pencil2d', 'cpp', version: '0.5.4', default_options: ['cpp_std=c++11', 'default_library=static'])

if meson.get_compiler('cpp').get_id() == 'msvc'
  add_project_arguments('/MP', language: 'cpp')
endif

sys_dep = []
if host_machine.system() == 'darwin'
  sys_dep = dependency('appleframeworks', modules: ['AppKit', 'Carbon'])
endif

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: [
  'Core',
  'Widgets',
  'Gui',
  'Xml',
  'XmlPatterns',
  'Multimedia',
  'Svg'
])

zlib_dep = dependency('zlib', fallback: ['zlib', 'zlib_dep'])

# Detection logic based on FindQuaZip.cmake
quazip_dep = dependency('quazip', required: false)
if not quazip_dep.found()
  cpp_compiler = meson.get_compiler('cpp')

  quazip_lib = cpp_compiler.find_library('quazip5', required: false)
  quazip_incdir = join_paths(get_option('prefix'), get_option('includedir'), 'quazip5')

  if quazip_lib.found() and cpp_compiler.has_header('quazip.h', args: '-I@0@'.format(quazip_incdir))
    quazip_dep = declare_dependency(include_directories: include_directories(quazip_incdir), dependencies: quazip_lib)
  else
    quazip_sp = subproject('quazip')
    quazip_dep = quazip_sp.get_variable('quazip_dep')
  endif
endif

subdir('core_lib')
subdir('app')
#subdir('tests')
