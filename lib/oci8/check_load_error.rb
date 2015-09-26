# This file is loaded only on LoadError.

class OCI8
  module Util

    case RUBY_PLATFORM
    when /mswin32|cygwin|mingw32|bccwin32/

      require 'Win32API'
      MAX_PATH = 260
      GetModuleFileNameA = Win32API.new('kernel32', 'GetModuleFileNameA', 'PPL', 'L')
      GetSystemDirectoryA = Win32API.new('kernel32', 'GetSystemDirectoryA', 'PL', 'L')
      GetWindowsDirectoryA = Win32API.new('kernel32', 'GetWindowsDirectoryA', 'PL', 'L')

      def self.check_os_specific_load_error(exc)
        case exc.message
        when /^193: / # "193: %1 is not a valid Win32 application." in English
          check_win32_pe_arch(exc.message.split(' - ')[1], "ruby-oci8")
          dll_load_path_list.each do |path|
            check_win32_pe_arch(File.join(path, '\OCI.DLL'), "Oracle client")
          end
        end
      end # self.check_os_specific_load_error

      def self.dll_load_path_list
        buf = "\0" * MAX_PATH
        paths = []
        paths << buf[0, GetModuleFileNameA.call(nil, buf, MAX_PATH)].force_encoding("locale").gsub(/\\[^\\]*$/, '')
        paths << buf[0, GetSystemDirectoryA.call(buf, MAX_PATH)].force_encoding("locale")
        paths << buf[0, GetWindowsDirectoryA.call(buf, MAX_PATH)].force_encoding("locale")
        paths << "."
        paths + ENV['PATH'].split(';')
      end # self.dll_load_path_list

      def self.check_win32_pe_arch(filename, package)
        open(filename, 'rb') do |f|
          # DOS header.
          if f.read(2) == 'MZ'
            f.seek(60, IO::SEEK_SET)
            pe_offset = f.read(4).unpack('V')[0]
            f.seek(pe_offset)
            # PE header.
            if f.read(4) == "PE\000\000"
              case f.read(2).unpack('v')[0]
              when 0x8664
                if [nil].pack('P').size == 4
                  raise LoadError, "\"#{filename}\" is x64 DLL. Use 32-bit #{package} instead."
                end
                return true
              when 0x014c
                if [nil].pack('P').size == 8
                  raise LoadError, "\"#{filename}\" is 32-bit DLL. Use x64 #{package} instead."
                end
                return true
              end
            end
          end
          raise LoadError, "\"#{filename}\" is not a valid Win32 application."
        end
        nil
      rescue
        nil
      end # self.check_win32_pe_arch

    when /linux/

      def self.check_os_specific_load_error(exc)
        case exc.message
        when /^libaio\.so\.1:/ # "libaio.so.1: cannot open shared object file: No such file or directory" in English
          install_cmd = if File.executable? '/usr/bin/apt-get'
                          'apt-get install libaio1'
                        elsif File.executable? '/usr/bin/yum'
                          'yum install libaio'
                        end
          if install_cmd
            raise LoadError, "You need to install libaio.so.1. Run '#{install_cmd}'."
          else
            raise LoadError, "You need to install libaio.so.1."
          end
        end
      end # self.check_os_specific_load_error

    else

      def self.check_os_specific_load_error(exc)
      end

    end # case RUBY_PLATFORM

    def self.check_load_error(exc)
      check_os_specific_load_error(exc)
      case exc.message
      when /^OCI Library Initialization Error/
        # TODO
      end
    end

  end # module Util
end
