project('minitpt', 'cpp',
  version : '0.1',
  default_options : ['cpp_std=c++11'])

# Hardcoded for now, we can use fancy Meson options for toggling "features" later
features = ['-DSDL_INC', '-DLIN', '-DGRAVFFT', '-D_REENTRANT', '-D_64BIT', '-DX86', '-DX86_SSE', '-DX86_SSE2', '-DX86_SSE3',
			'-ftree-vectorize', '-funsafe-math-optimizations', '-ffast-math', '-fomit-frame-pointer', '-funsafe-loop-optimizations']

add_global_arguments(['-march=native', '-pipe', '-O2', features], language : 'cpp')

# Ugly hack for now (well, not really) since Meson does not support wildcards (as to keep being "fast")
# we can replace this later with a hardcoded list of source files
sources = run_command('find', 'src', 'data', 'generated', '-type', 'f', '-name', '*.cpp').stdout().strip().split('\n')

include_dirs = include_directories('data/', 'generated/', 'src/')

# Find a way to better organize these, need some sort of automatic list generation, a Meson macro perhaps
mdep = meson.get_compiler('cpp').find_library('m')
# bz2bdep = meson.get_compiler('cpp').find_library('bz2')
# zlibdep = meson.get_compiler('cpp').find_library('libz')
fftwdep = meson.get_compiler('cpp').find_library('fftw3f')
sdldep = meson.get_compiler('cpp').find_library('SDL2')
pthreaddep = meson.get_compiler('cpp').find_library('pthread')

coredeps = [mdep, fftwdep, sdldep, pthreaddep]

executable('minitpt', sources, include_directories: include_dirs, dependencies: coredeps)
