angular.module('driverapp.services.api', [])

.factory('APISrv', function ($rootScope, $http, $q, Config, Utils, WSNSrv) {
    var ERROR_TYPE = 'errorType';
    var ERROR_MSG = 'errorMsg';

    var APIService = {};

    APIService.getSchools = function () {
        var deferred = $q.defer();

        $http.get(Config.SERVER_URL + '/school/' + Config.IDENTITY.OWNER_ID, Config.getHttpConfig())

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getRouteById = function (routeId) {
        var deferred = $q.defer();

        if (!routeId) {
            deferred.reject('Invalid routeId');
            return deferred.promise;
        }

        $http.get(Config.SERVER_URL + '/route/' + Config.IDENTITY.OWNER_ID + '/' + routeId, Config.getHttpConfig())

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };


    APIService.getRoutesBySchool = function (schoolId, date) {
        var deferred = $q.defer();

        if (!schoolId) {
            deferred.reject('Invalid schoolId');
            return deferred.promise;
        }

        var httpConfigWithParams = angular.copy(Config.getHttpConfig());
        httpConfigWithParams.params = {};

        if (Utils.isValidDate(date)) {
            httpConfigWithParams.params['date'] = date;
        }

        $http.get(Config.SERVER_URL + '/route/' + Config.IDENTITY.OWNER_ID + '/school/' + schoolId, httpConfigWithParams)

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getStopsByRoute = function (routeId) {
        var deferred = $q.defer();

        if (!routeId) {
            deferred.reject('Invalid routeId');
            return deferred.promise;
        }

        $http.get(Config.SERVER_URL + '/stop/' + Config.IDENTITY.OWNER_ID + '/' + routeId, Config.getHttpConfig())

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getChildrenBySchool = function (schoolId) {
        var deferred = $q.defer();

        if (!schoolId) {
            deferred.reject('Invalid schoolId');
            return deferred.promise;
        }

        $http.get(Config.SERVER_URL + '/child/' + Config.IDENTITY.OWNER_ID + '/' + schoolId, Config.getHttpConfig())

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getChildrenBySchoolAndClass = function (schoolId, classRoom) {
        var deferred = $q.defer();

        if (!schoolId) {
            deferred.reject('Invalid schoolId');
            return deferred.promise;
        } else if (!classRoom) {
            deferred.reject('Invalid classRoom');
            return deferred.promise;
        }

        var httpConfigWithParams = angular.copy(Config.getHttpConfig());
        httpConfigWithParams.params = {
            'classRoom': classRoom
        };

        $http.get(Config.SERVER_URL + '/child/' + Config.IDENTITY.OWNER_ID + '/' + schoolId + '/classroom', httpConfigWithParams)

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getChildImage = function (childId) {
        var deferred = $q.defer();

        if (!childId) {
            deferred.reject('Invalid childId');
            return deferred.promise;
        }

        if (ionic.Platform.isWebView()) {
            var sourceUrl = Config.SERVER_URL + '/image/download/jpg/' + Config.IDENTITY.OWNER_ID + '/' + childId;
            var targetFile = cordova.file.externalRootDirectory + Config.IMAGES_DIR + childId + '.jpg';

            var ft = new FileTransfer();
            ft.download(
                encodeURI(sourceUrl),
                targetFile,
                function (entry) {
                    console.log('download complete: ' + entry.toURL());
                    deferred.resolve(entry.toURL());
                },
                function (error) {
                    /*console.log('download error source ' + error.source);
                    console.log('download error target ' + error.target);
                    console.log('upload error code' + error.code);*/
                    deferred.reject();
                },
                false, {
                    headers: {
                        'X-ACCESS-TOKEN': Config.IDENTITY.X_ACCESS_TOKEN
                    }
                }
            );
        } else {
            deferred.reject();
        }

        return deferred.promise;
    };

    APIService.getAnchors = function () {
        var deferred = $q.defer();

        $http.get(Config.SERVER_URL + '/anchor/' + Config.IDENTITY.OWNER_ID, Config.getHttpConfig())

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getVolunteersBySchool = function (schoolId) {
        var deferred = $q.defer();

        if (!schoolId) {
            deferred.reject('Invalid schoolId');
            return deferred.promise;
        }

        $http.get(Config.SERVER_URL + '/volunteer/' + Config.IDENTITY.OWNER_ID + '/' + schoolId, Config.getHttpConfig())

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getVolunteersCalendarsBySchool = function (schoolId, dateFrom, dateTo) {
        var deferred = $q.defer();

        if (!schoolId) {
            deferred.reject('Invalid schoolId');
            return deferred.promise;
        } else if (!Utils.isValidDateRange(dateFrom, dateTo)) {
            deferred.reject('Invalid date range');
            return deferred.promise;
        }

        var httpConfigWithParams = angular.copy(Config.getHttpConfig());
        httpConfigWithParams.params = {
            'dateFrom': dateFrom,
            'dateTo': dateTo
        };

        $http.get(Config.SERVER_URL + '/volunteercal/' + Config.IDENTITY.OWNER_ID + '/' + schoolId, httpConfigWithParams)

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.getVolunteersCalendarsBySchoolAndVolunteer = function (schoolId, volunteerId, dateFrom, dateTo) {
        var deferred = $q.defer();

        if (!schoolId) {
            deferred.reject('Invalid schoolId');
            return deferred.promise;
        } else if (!volunteerId) {
            deferred.reject('Invalid volunteerId');
            return deferred.promise;
        } else if (!Utils.isValidDateRange(dateFrom, dateTo)) {
            deferred.reject('Invalid date range');
            return deferred.promise;
        }

        var httpConfigWithParams = angular.copy(Config.getHttpConfig());
        httpConfigWithParams.params = {
            'dateFrom': dateFrom,
            'dateTo': dateTo
        };

        $http.get(Config.SERVER_URL + '/volunteercal/' + Config.IDENTITY.OWNER_ID + '/' + schoolId + '/' + volunteerId, httpConfigWithParams)

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.addEvents = function (events) {
        var deferred = $q.defer();

        if (!events || events.length === 0) {
            deferred.reject('Invalid events');
        }

        $http.post(Config.EVENTS_SERVER_URL + '/event/' + Config.IDENTITY.OWNER_ID, events, Config.getHttpConfig())

        .then(
            function (response) {
                deferred.resolve(response.data);
            },
            function (reason) {
                deferred.reject('[' + reason.headers(ERROR_TYPE) + '] ' + reason.headers(ERROR_MSG));
            }
        );

        return deferred.promise;
    };

    APIService.uploadLog = function (fileURL, routeId) {
        var deferred = $q.defer();

        if (ionic.Platform.isWebView()) {
            if (!fileURL || fileURL.length === 0) {
                deferred.reject('Invalid fileURL');
            }

            //fileURL = cordova.file.externalRootDirectory + fileURL;
            fileURL = 'file://' + fileURL;

            var options = new FileUploadOptions();
            options.fileKey = 'file';
            options.fileName = fileURL.substr(fileURL.lastIndexOf('/') + 1);
            if (!!routeId) {
                options.fileName = routeId + '_' + options.fileName;
            }

            options.mimeType = 'text/plain';
            options.headers = {
                'X-ACCESS-TOKEN': Config.IDENTITY.X_ACCESS_TOKEN
            };
            options.params = {
                name: options.fileName
            };

            var serverURL = Config.EVENTS_SERVER_URL + '/log/upload/' + Config.IDENTITY.OWNER_ID;

            var ft = new FileTransfer();
            ft.upload(
                fileURL,
                encodeURI(serverURL),
                function (r) {
                    deferred.resolve(r.response);
                },
                function (error) {
                    deferred.reject(error);
                },
                options
            );
        } else {
            deferred.reject('cordova is not defined');
        }

        return deferred.promise;
    };

    APIService.uploadWsnLogs = function (routeId) {
        var deferred = $q.defer();

        if (ionic.Platform.isWebView()) {
            WSNSrv.getLogFiles().then(
                function (logFilesPaths) {
                    angular.forEach(logFilesPaths, function (logFilePath) {
                        APIService.uploadLog(logFilePath, routeId).then(
                            function (r) {
                                deferred.resolve(r);
                            },
                            function (error) {
                                deferred.reject(error);
                            }
                        );
                    });
                },
                function (reason) {
									deferred.reject(reason);
                }
            );
        } else {
            deferred.reject('cordova is not defined');
        }

        return deferred.promise;
    };

    return APIService;
});
