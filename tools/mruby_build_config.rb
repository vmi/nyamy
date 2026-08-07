# mruby_build_config.rb - MSVC build configuration for mruby
#
# Usage (called by build_mruby.ps1):
#   ruby minirake MRUBY_CONFIG=path\to\mruby_build_config.rb
#
# Which configs to build is controlled by the MRUBY_CONFIGS environment
# variable (comma-separated, e.g. "Release" or "Release,Debug").
# Defaults to "Release" when the variable is not set.
#
# Build outputs:
#   build/Release/lib/libmruby.lib   (/MD)
#   build/Debug/lib/libmruby.lib     (/MDd /Od /Zi)

configs = ENV.fetch('MRUBY_CONFIGS', 'Release').split(',').map(&:strip)

if configs.include?('Release')
	MRuby::Build.new('Release') do |conf|
		toolchain :visualcpp
		conf.cc.flags  << '/MD' << '/utf-8'
		# enable_cxx_exception compiles the .c sources as C++; mruby uses C99
		# designated initializers (e.g. MRB_MT_ENTRY), which MSVC only accepts
		# in C++ mode from /std:c++20 onwards.
		conf.cxx.flags << '/MD' << '/utf-8' << '/std:c++20'
		conf.enable_cxx_exception
		conf.linker.flags << '/MACHINE:X64'
		conf.gembox 'full-core'
	end
end

if configs.include?('Debug')
	MRuby::Build.new('Debug') do |conf|
		toolchain :visualcpp
		# Remove Release-mode flags added by the toolchain before appending Debug ones.
		[conf.cc, conf.cxx].each do |cc|
			cc.flags.reject! { |f| f == '/MD' || f == '/O2' }
			cc.flags << '/MDd' << '/Od' << '/Zi' << '/utf-8'
		end
		# See the Release config: /std:c++20 is required once the C sources are
		# compiled as C++ by enable_cxx_exception.
		conf.cxx.flags << '/std:c++20'
		conf.enable_cxx_exception
		conf.linker.flags << '/MACHINE:X64'
		conf.gembox 'full-core'
	end
end
