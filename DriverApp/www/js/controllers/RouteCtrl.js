angular.module('driverapp.controllers.route', [])
  .controller('RouteCtrl', function ($scope, $rootScope, $stateParams, $ionicHistory, $ionicNavBarDelegate, $ionicPopup, $ionicModal, $interval, $ionicScrollDelegate, $filter, $timeout, $cordovaFile, $window, Config, Utils, StorageSrv, GeoSrv, AESrv, APISrv, WSNSrv) {
    $scope.fromWizard = false
    $rootScope.pedibusEnabled = true

    var passengersScrollDelegate = $ionicScrollDelegate.$getByHandle('passengersHandle')
    $scope.mergedOnBoard = []

    /*
     * Passengers panel size calculator
     */
    $scope.resizePassengersPanel = function () {
      $scope.passengersPanelStyle = {}
      var height = $window.innerHeight - (44 * 3)
      var onBoardHeight = 100 * ($scope.mergedOnBoard.length > 2 ? 2 : $scope.mergedOnBoard.length)
      height = height - onBoardHeight - (onBoardHeight > 0 ? 28 : 0)
      $scope.passengersPanelStyle['height'] = height + 'px'
    }

    var aesInstance = {}

    $scope.children = null
    $scope.driver = null
    $scope.helpers = []
    $scope.helpersTemp = []

    $rootScope.route = {}
    $scope.stops = []
    $scope.onBoardTemp = []
    $scope.onBoard = []

    $scope.viewPos = 0
    $scope.enRoutePos = 0
    $scope.enRouteArrived = false

    $scope.sel = {
      stop: null
    }

    var lastEventTimestamp = new Date().getTime()

    /* INIT */
    if ($stateParams['fromWizard']) {
      $scope.fromWizard = $stateParams['fromWizard']
    }

    if ($scope.fromWizard) {
      $rootScope.pedibusEnabled = false

      if ($stateParams['route']) {
        $rootScope.route = $stateParams['route']

        aesInstance = AESrv.startInstance($rootScope.route.objectId)
        WSNSrv.updateNodeList(StorageSrv.getChildren(), 'child', true)

        if ($stateParams['driver']) {
          $scope.driver = $stateParams['driver']
          AESrv.setDriver($scope.driver)
        }

        if ($stateParams['helpers']) {
          $scope.helpersTemp = $stateParams['helpers']
          /*
          $scope.helpers.forEach(function (helper) {
              AESrv.setHelper(helper);
          });
          */
        }

        if (!$scope.enRoutePos) {
          GeoSrv.startWatchingPosition(function (position) {
            if (!!position && !!position.coords) {
              var lat = position.coords.latitude
              var lon = position.coords.longitude
              var acc = position.coords.accuracy
              AESrv.driverPosition($scope.driver, lat, lon, acc)
            }
          }, null, Config.GPS_DELAY)
        }

        /*
         * WSN watch
         */
        $scope.$watch(
          function () {
            return WSNSrv.networkState
          },
          function (newNs, oldNs) {
            var ns = angular.copy(newNs)
            var somethingChanged = false
            $scope.batteryAlarm = false

            Object.keys(ns).forEach(function (nodeId) {
              if (WSNSrv.isNodeByType(nodeId, 'child')) {
                if (ns[nodeId].batteryLevel < 2) {
                  $scope.batteryAlarm = true
                }

                if (ns[nodeId].status === WSNSrv.STATUS_NEW) {
                  // new node?
                  if (!oldNs[nodeId].status) {
                    AESrv.nodeInRange(ns[nodeId].object)
                  }

                  var child = $scope.isChildOfThisStop(nodeId)
                  if (child !== null) {
                    $scope.takeOnBoard(child.objectId)
                    ns[nodeId].status = WSNSrv.STATUS_BOARDED_ALREADY
                    somethingChanged = true
                  }
                }

                if ($scope.isOnBoard(ns[nodeId].object.objectId)) {
                  if (ns[nodeId].timestamp === -1) {
                    return
                  }

                  var overTimeout = (moment().valueOf() - ns[nodeId].timestamp) > Config.NODESTATE_TIMEOUT

                  if (overTimeout && ns[nodeId].status !== WSNSrv.STATUS_OUT_OF_RANGE) {
                    ns[nodeId].status = WSNSrv.STATUS_OUT_OF_RANGE
                    somethingChanged = true
                    AESrv.nodeOutOfRange(ns[nodeId].object, ns[nodeId].timestamp)
                    console.log('nodeOutOfRange: ' + nodeId + ' (last seen ', +ns[nodeId].timestamp + ')')
                    /*
                    var errorString = 'Attenzione! '
                    errorString += ns[nodeId].object.surname + ' ' + ns[nodeId].object.name + ' (' + nodeId + ') fuori portata!'
                    // TODO toast for failure
                    Utils.toast(errorString, 5000, 'center');
                    */
                  } else if (!overTimeout && ns[nodeId].status === WSNSrv.STATUS_OUT_OF_RANGE) {
                    ns[nodeId].status = WSNSrv.STATUS_BOARDED_ALREADY
                    somethingChanged = true
                    AESrv.nodeInRange(ns[nodeId].object)
                    console.log('nodeBackInRange: ' + nodeId)
                    /*
                    var toastString = ns[nodeId].object.surname + ' ' + ns[nodeId].object.name + ' (' + nodeId + ') di nuovo in portata!'
                    // TODO toast for failure
                    Utils.toast(toastString, 5000, 'center');
                    */
                  }
                }
              }
            })

            if (somethingChanged) {
              WSNSrv.networkState = ns
            }
          }
        )
      }

      $scope.resizePassengersPanel()
    } else {
      if ($stateParams['route']) {
        $rootScope.route = $stateParams['route']
      } else if ($stateParams['routeId']) {
        $rootScope.route = StorageSrv.getRouteById($stateParams['routeId'])
      }
    }

    /*
     * populate stops
     */
    Utils.loading()
    APISrv.getStopsByRoute($stateParams['routeId']).then(
      function (stops) {
        $scope.stops = stops
        // get children images
        if (Utils.isConnectionFastEnough()) {
          // download children images
          var counter = 0
          var downloaded = 0
          var children = StorageSrv.getChildren()
          var childrenByRoute = []
          angular.forEach($scope.stops, function (stop) {
            angular.forEach(children, function (child) {
              if (stop.passengerList.indexOf(child.objectId) !== -1) {
                childrenByRoute.push(child)
              }
            })
          })
          angular.forEach(childrenByRoute, function (child) {
            APISrv.getChildImage(child.objectId).then(
              function () {
                downloaded++
                counter++
                console.log('Downloaded images: ' + downloaded + ' (Total: ' + counter + '/' + children.length + ')')
                if (counter === childrenByRoute.length) {
                  // Select the first automatically
                  $scope.sel.stop = $scope.stops[0]
                  $scope.getChildrenForStop($scope.sel.stop)
                  Utils.loaded()
                }
              },
              function () {
                counter++
                if (counter === childrenByRoute.length) {
                  // Select the first automatically
                  $scope.sel.stop = $scope.stops[0]
                  $scope.getChildrenForStop($scope.sel.stop)
                  Utils.loaded()
                }
              }
            )
          })
        } else {
          // Select the first automatically
          $scope.sel.stop = $scope.stops[0]
          $scope.getChildrenForStop($scope.sel.stop)
          Utils.loaded()
        }
      },
      function (error) {
        Utils.loaded()
        console.log(error)
      }
    )

    $scope.viewPrevious = function () {
      if (($scope.viewPos - 1) >= 0) {
        $scope.viewPos--
        $scope.sel.stop = $scope.stops[$scope.viewPos]
        $scope.getChildrenForStop($scope.sel.stop)
      }
    }

    $scope.viewNext = function () {
      if (($scope.viewPos + 1) <= $scope.enRoutePos) {
        $scope.viewPos++
        $scope.sel.stop = $scope.stops[$scope.viewPos]
        $scope.getChildrenForStop($scope.sel.stop)
      }
    }

    var handleChildrenAndHelpers = function () {
      // NODE_CHECKIN
      /*
      $scope.onBoardTemp.forEach(function (passengerId) {
          var child = $scope.getChild(passengerId);
          AESrv.nodeCheckin(child);
          if (!!child.wsnId) {
              WSNSrv.checkinChild(child.wsnId);
          }
      });
      */

      // update onBoard
      $scope.onBoard = $scope.onBoard.concat($scope.onBoardTemp)
      $scope.onBoardTemp = []
      $scope.updateMergedOnBoard()

      // add new helpers
      $scope.helpersTemp.forEach(function (helper) {
        AESrv.setHelper(helper)
      })
      $scope.helpers = $scope.helpers.concat($scope.helpersTemp)
      $scope.helpersTemp = []
    }

    $scope.goNext = function (event) {
      var eventTimestamp = event.timeStamp
      // drop multi-event
      if (eventTimestamp < (lastEventTimestamp + 1500)) {
        console.log('event discarded')
        // Utils.toast("event discarded");
        return
      }
      lastEventTimestamp = eventTimestamp

      $ionicScrollDelegate.scrollTop(true)

      // if has next
      if ($scope.stops[$scope.enRoutePos + 1]) {
        // $scope.enRoute = !$scope.enRoute;

        // first leave
        if (!$scope.enRoutePos) {
          AESrv.startRoute($scope.stops[$scope.enRoutePos])
          // timer for automatic arrive
          console.log('Automatic "Arrive" timeout started!')
          $timeout(function () {
            // Arriva in maniera automatica
            if (!$scope.enRouteArrived) {
              $scope.nextStop()
              console.log('Automatically arrived!')
            } else {
              console.log('Automatic "Arrive" not necessary')
            }
          }, Config.AUTOFINISH_DELAY)
        }

        handleChildrenAndHelpers()

        // move to the next
        AESrv.stopLeaving($scope.sel.stop)
        $scope.enRoutePos++
        $scope.viewPos++
        $scope.sel.stop = $scope.stops[$scope.enRoutePos]
        $scope.getChildrenForStop($scope.sel.stop)
      } else {
        // Arriva
        $ionicPopup.confirm({
          title: 'INVIA DATI',
          template: 'Cliccando su INVIA i dati verranno mandati al server e, per questa sessione, non potrai più modificare lo stato dei passeggeri. Confermi di aver concluso la corsa del pedibus e di voler inviare i dati?',
          cssClass: 'route-popup',
          cancelText: 'Annulla',
          cancelType: 'button-stable',
          okText: 'INVIA',
          okType: 'button-positive'
        }).then(function (ok) {
          if (ok) {
            handleChildrenAndHelpers()

            var childrenWsnIds = []
            $scope.onBoard.forEach(function (passengerId) {
              var child = $scope.getChild(passengerId)
              AESrv.nodeAtDestination(child)
              AESrv.nodeCheckout(child)

              if (child.wsnId && WSNSrv.networkState[child.wsnId]) {
                AESrv.batteryStatus(child)

                if (WSNSrv.networkState[child.wsnId].status === WSNSrv.STATUS_BOARDED_ALREADY) {
                  // WSNSrv.checkoutChild(child.wsnId);
                  childrenWsnIds.push(child.wsnId)
                }
              }
            })

            if (childrenWsnIds.length > 0) {
              WSNSrv.checkoutChildren(childrenWsnIds)
            }

            AESrv.endRoute($scope.stops[$scope.enRoutePos])

            GeoSrv.stopWatchingPosition()
            WSNSrv.stopListener()

            $scope.enRouteArrived = true
            $rootScope.pedibusEnabled = true
          }
        })
      }
    }

    $scope.getChild = function (childId) {
      var foundChild = null
      for (var i = 0; i < $scope.children.length; i++) {
        var child = $scope.children[i]
        if (child.objectId === childId) {
          foundChild = child
          i = $scope.children.length
        }
      }
      return foundChild
    }

    $scope.getChildBg = function (child) {
      if (child.imageUrl) {
        return {
          'background-image': 'url(\'' + child.imageUrl + '\')'
        }
      }
      return {}
    }

    $scope.getChildrenForStop = function (stop) {
      // $scope.sel.stop = stop;
      var passengers = []
      if ($scope.children == null || !$scope.children.length) {
        $scope.children = StorageSrv.getChildren()
      }
      if ($scope.children != null) {
        $scope.children.forEach(function (child) {
          // set image path
          if (ionic.Platform.isWebView()) {
            var imagePath = cordova.file.externalRootDirectory + Config.IMAGES_DIR + child.objectId + '.jpg'
            $cordovaFile.checkFile(cordova.file.externalRootDirectory + Config.IMAGES_DIR, child.objectId + '.jpg').then(
              function (success) {
                child.imageUrl = imagePath
              }
            )
          }

          if (stop.passengerList.indexOf(child.objectId) !== -1) {
            passengers.push(child)
          }
        })
      }
      stop.passengers = passengers
    }

    $scope.isChildOfThisStop = function (childWsnId) {
      var child = null
      if (!!$scope.sel.stop && !!$scope.sel.stop.passengers) {
        for (var i = 0; i < $scope.sel.stop.passengers.length; i++) {
          var passenger = $scope.sel.stop.passengers[i]
          if (passenger.wsnId === childWsnId) {
            child = passenger
            i = $scope.sel.stop.passengers.length
          }
        }
      }
      return child
    }

    $scope.takeOnBoard = function (passengerId) {
      if ($scope.onBoardTemp.indexOf(passengerId) === -1) {
        $scope.onBoardTemp.push(passengerId)
        var child = $scope.getChild(passengerId)
        AESrv.nodeCheckin(child)
        if (child.wsnId) {
          WSNSrv.checkinChild(child.wsnId)
        }
      }

      $scope.updateMergedOnBoard()
      passengersScrollDelegate.resize()
    }

    $scope.dropOff = function (passenger, event) {
      event.stopPropagation()

      var child = $scope.getChild(passenger.id)
      if (passenger.tmp) {
        var index = $scope.onBoardTemp.indexOf(passenger.id)
        if (index !== -1) {
          $scope.onBoardTemp.splice(index, 1)
          AESrv.nodeCheckout(child)
          $scope.updateMergedOnBoard()
          passengersScrollDelegate.resize()
        }
      } else {
        $ionicPopup.confirm({
          title: 'Rimuovi ' + child.surname + ' ' + child.name,
          template: child.surname + ' ' + child.name + ' è stato aggiunto in una fermata precedente. Vuoi confermare la rimozione?',
          cssClass: 'route-popup',
          cancelText: 'Annulla',
          cancelType: 'button-stable',
          okText: 'RIMUOVI',
          okType: 'button-positive'
        }).then(function (ok) {
          if (ok) {
            var index = $scope.onBoard.indexOf(passenger.id)
            if (index !== -1) {
              $scope.onBoard.splice(index, 1)
              AESrv.nodeCheckout(child)
              $scope.updateMergedOnBoard()
              passengersScrollDelegate.resize()
            }
          }
        })
      }
    }

    $scope.toBeTaken = []
    $scope.setToBeTaken = function (child) {
      var tbtIndex = $scope.toBeTaken.indexOf(child.objectId)
      if (!!child.checked && tbtIndex === -1) {
        $scope.toBeTaken.push(child.objectId)
      } else if (!child.checked && tbtIndex !== -1) {
        $scope.toBeTaken.splice(tbtIndex, 1)
      }
    }

    $scope.isOnBoard = function (childId) {
      if ($scope.onBoard.indexOf(childId) !== -1 || $scope.onBoardTemp.indexOf(childId) !== -1) {
        return true
      }
      return false
    }

    /*
     * "Add others" Popup
     */
    $scope.selectChildrenPopup = function () {
      // var childrenPopup =
      $ionicPopup.show({
        templateUrl: 'templates/route_popup_children.html',
        cssClass: 'route-popup',
        title: 'BAMBINI',
        subTitle: $rootScope.route.name,
        scope: $scope,
        buttons: [{
          text: 'CHIUDI',
          type: 'button-stable',
          onTap: function () {
            // reset
            $scope.toBeTaken.forEach(function (childId) {
              $scope.getChild(childId).checked = false
            })
            $scope.toBeTaken = []

            $scope.updateMergedOnBoard()
            passengersScrollDelegate.resize()
          }
        }, {
          text: 'SALVA',
          type: 'button-positive',
          onTap: function (e) {
            if ($scope.toBeTaken.length > 0) {
              var wsnIds = []
              $scope.toBeTaken.forEach(function (passengerId) {
                var child = $scope.getChild(passengerId)
                AESrv.nodeCheckin(child)
                if (child.wsnId) {
                  wsnIds.push(child.wsnId)
                }
              })
              if (wsnIds.length > 0) {
                WSNSrv.checkinChildren(wsnIds)
              }

              $scope.onBoardTemp = $scope.onBoardTemp.concat($scope.toBeTaken)
            }

            // reset
            $scope.toBeTaken.forEach(function (childId) {
              $scope.getChild(childId).checked = false
            })
            $scope.toBeTaken = []

            $scope.updateMergedOnBoard()
            passengersScrollDelegate.resize()
          }
        }]
      })
    }

    /*
     * Child details popup
     */
    $scope.showChildDetails = function (child) {
      $scope.phone = (child.parentName ? child.parentName + ': ' + child.phone : child.phone)

      // var detailsPopup =
      $ionicPopup.alert({
        title: child.surname + ' ' + child.name,
        // template: (!!child.parentName ? child.parentName + ': ' + child.phone : child.phone),
        templateUrl: 'templates/phone_popup_person.html',
        scope: $scope,
        okText: 'OK',
        okType: 'button'
      })
    }

    /*
     * Merge lists method: used to merge the two lists (onBoard and onBoardTmp) and generate a matrix with rows of 3 cols
     */
    $scope.updateMergedOnBoard = function () {
      var onBoardMerged = []
      var onBoardMatrix = []
      var cols = 3
      for (var i = 0; i < $scope.onBoard.length; i++) {
        var tmpData = {
          id: $scope.onBoard[i],
          tmp: false
        }
        onBoardMerged.push(tmpData)
      }
      for (var j = 0; j < $scope.onBoardTemp.length; j++) {
        if ($scope.isNewValue(onBoardMerged, tmpData)) {
          onBoardMerged.push({
            id: $scope.onBoardTemp[j],
            tmp: true
          })
        }
      }
      // here I have to convert in matrix
      for (var k = 0; k < onBoardMerged.length; k += cols) {
        var rowArr = []
        var actualCols = (k + cols)
        if ((actualCols) <= onBoardMerged.length) {
          for (var c = 0; c < cols; c++) {
            rowArr.push(onBoardMerged[k + c])
          }
        } else {
          for (var d = k; d < onBoardMerged.length; d++) {
            rowArr.push(onBoardMerged[d])
          }
        }
        onBoardMatrix.push(rowArr)
      }

      $scope.mergedOnBoard = onBoardMatrix
      $scope.resizePassengersPanel()
      passengersScrollDelegate.resize()
    }

    /*
     * Check lists method: used to check if a value is already present in a list or not
     */
    $scope.isNewValue = function (arr, val) {
      var present = false
      for (var i = 0; i < arr.length && !present; i++) {
        if (arr[i] === val) {
          present = true
        }
      }
      return !present
    }

    $scope.isRoutePanelOpen = false // initial state of the route panel (closed)
    $scope.openRouteView = function () {
      $scope.isRoutePanelOpen = true
    }

    $scope.closeRouteView = function () {
      $scope.isRoutePanelOpen = false
    }

    $scope.selectHelpersPopup = function () {
      var calendars = StorageSrv.getVolunteersCalendars()
      var sortedVolunteers = $filter('orderBy')(StorageSrv.getVolunteers(), ['checked', 'name'])
      // sort using calendars
      for (var j = 0; j < calendars.length; j++) {
        var cal = calendars[j]
        if (cal.schoolId === StorageSrv.getSchoolId() && cal.routeId === $rootScope.route.objectId) {
          // helpers
          if (!!cal.helperList && cal.helperList.length > 0) {
            var counter = 0
            cal.helperList.forEach(function (helperId) {
              for (var i = 0; i < sortedVolunteers.length; i++) {
                if (sortedVolunteers[i].objectId === helperId) {
                  Utils.moveInArray(sortedVolunteers, i, counter)
                  // console.log('Helper ' + sortedVolunteers[counter].name + ' moved to position ' + counter);
                  counter++
                  i = sortedVolunteers.length
                }
              }
            })
          }
          j = calendars.length
        }
      }
      $scope.volunteers = sortedVolunteers

      $scope.helpers = $filter('orderBy')($scope.helpers, ['checked', 'name'])
      $scope.helpersTemp = $filter('orderBy')($scope.helpersTemp, ['checked', 'name'])
      var hs = $scope.helpers.concat($scope.helpersTemp)

      var oldHelpersIds = []
      $scope.helpers.forEach(function (h) {
        oldHelpersIds.push(h.objectId)
      })

      var volunteersCounter = 0
      hs.forEach(function (h) {
        for (var i = 0; i < $scope.volunteers.length; i++) {
          if ($scope.volunteers[i].objectId === h.objectId) {
            $scope.volunteers[i].checked = true
            if (oldHelpersIds.indexOf($scope.volunteers[i].objectId) !== -1) {
              $scope.volunteers[i].disabled = true
            }
            Utils.moveInArray($scope.volunteers, i, volunteersCounter)
            // console.log('Helper ' + $scope.volunteers[countesendDataPopupr].name + ' moved to position ' + volunteersCounter);
            volunteersCounter++
            i = $scope.volunteers.length
          }
        }
      })

      var helpersPopup = $ionicPopup.show({
        templateUrl: 'templates/route_popup_helpers.html',
        cssClass: 'route-popup',
        title: 'ACCOMPAGNATORI',
        subTitle: $rootScope.route.name,
        scope: $scope,
        buttons: [{
          text: 'CHIUDI',
          type: 'button-stable'
        }, {
          text: 'SALVA',
          type: 'button-positive',
          onTap: function (e) {
            $scope.selectHelpers($scope.volunteers)
          }
        }]
      })

      $scope.selectHelpers = function (volunteers) {
        $scope.helpersTemp = []
        for (var i = 0; i < volunteers.length; i++) {
          if (volunteers[i].checked && !volunteers[i].disabled) {
            $scope.helpersTemp.push(volunteers[i])
          }
        }
        // $scope.helpers = $scope.helpersTemp; // here I align the helpers
        helpersPopup.close()
      }
    }
  })
