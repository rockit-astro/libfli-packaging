Name:      rasa-libfli
Version:   1.104
Release:   0
Url:       https://github.com/warwick-one-metre/libfli
Summary:   Repackaged FLI SDK library
License:   Unspecified
Group:     Unspecified

%description

Part of the observatory software for the RASA prototype telescope.

%build
mkdir -p %{buildroot}%{_libdir}

%{__install} %{_sourcedir}/libfli.so %{buildroot}%{_libdir}

%files
%defattr(0644,root,root,-)
%{_libdir}/libfli.so
%changelog
