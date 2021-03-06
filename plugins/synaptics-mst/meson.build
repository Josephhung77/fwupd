if get_option('plugin_synaptics_mst')
if not get_option('gudev')
  error('gudev is required for plugin_synaptics_mst')
endif
cargs = ['-DG_LOG_DOMAIN="FuPluginSynapticsMST"']

install_data(['synaptics-mst.quirk'],
  install_dir: join_paths(datadir, 'fwupd', 'quirks.d')
)

shared_module('fu_plugin_synaptics_mst',
  fu_hash,
  sources : [
    'fu-plugin-synaptics-mst.c',
    'fu-synaptics-mst-common.c',
    'fu-synaptics-mst-connection.c',
    'fu-synaptics-mst-device.c',
  ],
  include_directories : [
    root_incdir,
    fwupd_incdir,
    fwupdplugin_incdir,
  ],
  install : true,
  install_dir: plugin_dir,
  c_args : [
    cargs,
  ],
  link_with : [
    fwupd,
    fwupdplugin,
  ],
  dependencies : [
    plugin_deps,
  ],
)

if get_option('tests')
  testdatadirs = environment()
  testdatadirs.set('G_TEST_SRCDIR', meson.current_source_dir())
  testdatadirs.set('G_TEST_BUILDDIR', meson.current_build_dir())
  testdatadirs.set('FWUPD_LOCALSTATEDIR', '/tmp/fwupd-self-test/var')
  e = executable(
    'synaptics-mst-self-test',
    fu_hash,
    sources : [
      'fu-self-test.c',
      'fu-synaptics-mst-common.c',
      'fu-synaptics-mst-connection.c',
      'fu-synaptics-mst-device.c',
    ],
    include_directories : [
      root_incdir,
      fwupd_incdir,
      fwupdplugin_incdir,
    ],
    dependencies : [
      plugin_deps,
      sqlite,
      valgrind,
    ],
    link_with : [
      fwupd,
      fwupdplugin,
    ],
    c_args : [
      cargs,
    ],
    install : true,
    install_dir : installed_test_bindir,
  )
  test('synaptics-mst-self-test', e, env: testdatadirs)
endif
endif
