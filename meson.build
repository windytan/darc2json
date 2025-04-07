project(
  'darc2json',
  'cpp',
  default_options: [
    'warning_level=3',
    'buildtype=release',
    'optimization=3',
    'prefix=/usr/local',
    'cpp_std=c++17',
  ],
  version: '0.2-SNAPSHOT',
)

# Store version number to be compiled in
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: conf)

########################
### Compiler options ###
########################

cc = meson.get_compiler('cpp')
add_project_arguments(cc.get_supported_arguments(['-Wno-unknown-pragmas']), language: 'cpp')

# Explicit GNU extensions on Cygwin
if build_machine.system() == 'cygwin'
  override_options = ['cpp_std=gnu++17']
else
  override_options = []
endif

####################
### Dependencies ###
####################

# Find libsndfile
sndfile = dependency('sndfile')

# Find nlohmann's json
json = dependency('nlohmann_json', version: '>=3.9.0')

# Find liquid-dsp
liquid = cc.find_library('liquid', required: false)
# macOS: The above mechanism sometimes fails, so let's look deeper
if not liquid.found() and build_machine.system() == 'darwin'
  fs = import('fs')
  brew = find_program('brew', required: false)
  if brew.found()
    # Homebrew system
    liquid_prefix = run_command(brew, '--prefix', 'liquid-dsp', check: true).stdout().strip()
    liquid_lib = cc.find_library('liquid', dirs: [liquid_prefix + '/lib'])
    liquid_inc = include_directories(liquid_prefix + '/include')
    liquid = declare_dependency(dependencies: liquid_lib, include_directories: liquid_inc)
  elif fs.is_dir('/opt/local/lib')
    # MacPorts system
    liquid_lib = cc.find_library('liquid', dirs: ['/opt/local/lib'])
    liquid_inc = include_directories('/opt/local/include')
    liquid = declare_dependency(dependencies: liquid_lib, include_directories: liquid_inc)
  endif
endif
# API for modem/modemcf changed recently, but we can deal with either
if liquid.found() and cc.has_function('modemcf_create', prefix: '#include <liquid/liquid.h>', dependencies: liquid)
  add_project_arguments('-DMODEM_IS_MODEMCF', language: 'cpp')
endif

############################
### Sources & Executable ###
############################

sources_no_main = [
  'src/darc2json.cc',
  'src/input.cc',
  'src/layer1.cc',
  'src/layer2.cc',
  'src/layer3_4.cc',
  'src/liquid_wrappers.cc',
  'src/util.cc',
]

executable(
  'darc2json',
  [sources_no_main, 'src/darc2json.cc'],
  dependencies: [json, liquid, sndfile],
  install: true,
  override_options: override_options,
)
