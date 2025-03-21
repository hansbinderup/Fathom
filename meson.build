project('fathom', 'c',
  version: '1.0',
  default_options: ['c_std=gnu99', 'warning_level=2']
)

src_dir = 'src'

c_args = ['-w', '-DTB_NO_HELPER_API']
include_dirs = include_directories(src_dir)

sources = [
  src_dir + '/tbprobe.c',
  src_dir + '/tbchess.c'
]

libfathom = static_library('fathom',
  sources,
  include_directories: include_dirs,
  c_args: c_args,
  pic: true
)

fathom_dep = declare_dependency(
  link_with: libfathom,
  include_directories: include_dirs
)
