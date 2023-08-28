Name:      libfli
Version:   1.104
Release:   0
Url:       https://github.com/rockit-astro/libfli-packaging
Summary:   Repackaged FLI SDK library
License:   Unspecified
Group:     Unspecified
BuildArch: x86_64

%description

FLI SDK repackaged for Rocky Linux.

%build
mkdir -p %{buildroot}%{_libdir}

%{__install} %{_sourcedir}/libfli.so %{buildroot}%{_libdir}

%files
%defattr(0644,root,root,-)
%{_libdir}/libfli.so

%changelog
