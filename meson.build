project('minitpt', ['cpp','c'],
  version : '0.1',
  default_options : ['cpp_std=c++11'])

# Hardcoded for now, we can use fancy Meson options for toggling "features" later
features = ['-DSDL_INC', '-DLIN', '-DGRAVFFT', '-D_REENTRANT', '-D_64BIT', '-DDEBUG', '-DX86', '-DX86_SSE', '-DX86_SSE2', '-DX86_SSE3',
			'-ffast-math', '-fomit-frame-pointer', '-Wno-invalid-offsetof', '-Ofast', '-march=native']

compiler = meson.get_compiler('cpp')

common = ['-funsafe-math-optimizations', '-ftree-vectorize']

# Enable various compiler diagnostics 
if get_option('compiler_info')
	if compiler.get_id() == 'intel'
		features += ['-diag-file-append=diag.txt', '-wd858']
	elif compiler.get_id() == 'gcc'
		features += ['-fopt-info-vec-missed=opts.txt']
	elif compiler.get_id() == 'clang'
		features += ['-Rpass-analysis=loop-vectorize', '-fsave-optimization-record', '-foptimization-record-file=opt.txt']
	endif
endif

# Set various compiler specific flags
if compiler.get_id() == 'gcc'
	features += ['-funsafe-loop-optimizations']
elif compiler.get_id() == 'clang'
	features += ['-flto=thin']
endif

features += common

add_global_arguments(['-march=native', '-pipe', '-O2', features], language : 'cpp')

# Ugly hack for now (well, not really) since Meson does not support wildcards (as to keep being "fast")
# we can replace this later with a hardcoded list of source files
sources = run_command('find', 'src', 'data', 'generated', '-type', 'f', '-name', '*.c*').stdout().strip().split('\n')

include_dirs = include_directories('data/', 'generated/', 'src/', 'imgui/')

# Find a way to better organize these, need some sort of automatic list generation, a Meson macro perhaps
mdep = meson.get_compiler('cpp').find_library('m')
dldep = meson.get_compiler('cpp').find_library('dl')
fftwdep = meson.get_compiler('cpp').find_library('fftw3f')
sdldep = meson.get_compiler('cpp').find_library('SDL2')
glewdep = dependency('glew')
glfwdep = meson.get_compiler('cpp').find_library('glfw')
pthreaddep = meson.get_compiler('cpp').find_library('pthread')

coredeps = [mdep, dldep, fftwdep, sdldep, glewdep, glfwdep, pthreaddep]

executable('minitpt', sources, include_directories: include_dirs, dependencies: coredeps)

# Cppcheck target
run_target('cppcheck', command : 'static_check.sh') 
