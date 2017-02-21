project('glrucache', 'c')

glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')

glrucache_lib = library('glru', 'g-lru-cache.c',
                        dependencies: [glib_dep, gobject_dep],
                        install: true)
glrucache_dep = declare_dependency(include_directories: include_directories('.'),
                                   link_with: glrucache_lib)

install_headers('g-lru-cache.h', subdir: 'glrucache')
install_data('glrucache.vapi', install_dir: 'share/vala/vapi')

pkgconfig = import('pkgconfig')
pkgconfig.generate(name: 'glrucache',
                   description: 'A quick LRU cache for GLib/GObject.',
                   version: '0.0.0',
                   libraries: [glrucache_lib],
                   filebase: 'glrucache',
                   subdirs: 'glrucache')
