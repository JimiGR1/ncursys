project('ncursys', 'c', version: '1.0.3')

glib_dep = dependency('glib-2.0')
ncurses_dep = dependency('ncursesw')
cc = meson.get_compiler('c')
math_dep = cc.find_library('m', required: false)

sources = [
  'main.c',
  'path_data.c',
  'tree_view.c',
  'path_tree.c',
  'helpers.c'
]

executable('ncursys',
  sources,
  dependencies: [glib_dep, ncurses_dep, math_dep],
  install: true
)

install_man('ncursys.1')