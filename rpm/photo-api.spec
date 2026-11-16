# SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
#
# SPDX-License-Identifier: BSD-3-Clause

Name:           photo-api
Summary:        Sailfish OS camera API
Version:        0.1.0
Release:        1
License:        BSD-3-Clause
URL:            https://github.com/sailfishos/photo-api
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  meson
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  python3-yaml

%description
High-level API for accessing the camera using various backends

%package devel
Summary: Development files for photo-api
Requires: photo-api = %{version}-%{release}

%description devel
%{summary}

%package qt5
Summary: QtQuick 5 QML wrapper for photo-api
Requires: photo-api = %{version}-%{release}

%description qt5
%{summary}

%package tools
Summary: Tools for photo-api
Requires: photo-api = %{version}-%{release}

%description tools
%{summary}

%prep
%setup -q

%build
%meson
%meson_build

%install
%meson_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license LICENSES/BSD-3-Clause.txt
%{_libdir}/libphoto-api-0.1.so.*

%files devel
%{_includedir}/photo-api-0.1/
%{_libdir}/libphoto-api-0.1.so
%{_libdir}/pkgconfig/photo-api-0.1.pc

%files qt5
%dir %{_libdir}/qt5/qml/org/sailfishos/PhotoApi
%{_libdir}/qt5/qml/org/sailfishos/PhotoApi/libqmlphotoapi.so
%{_libdir}/qt5/qml/org/sailfishos/PhotoApi/qmldir

%files tools
%{_bindir}/*
