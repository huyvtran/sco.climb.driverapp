package it.smartcommunitylab.climb.dap;

import org.apache.cordova.CallbackContext;
import org.apache.cordova.CordovaInterface;
import org.apache.cordova.CordovaPlugin;
import org.apache.cordova.CordovaWebView;
import org.apache.cordova.PluginResult;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.util.Log;
import fbk.climblogger.ClimbServiceInterface;
import fbk.climblogger.ClimbServiceInterface.NodeState;
import fbk.climblogger.ClimbSimpleService;

public class DriverAppPlugin extends CordovaPlugin {
	private static final String LOG_TAG = "ClimbDriverAppPlugin";

	private Context ctx;

	private ClimbSimpleService mClimbService = null;
	private boolean isBound = false;

	private BroadcastReceiver receiver = null;
	private CallbackContext listenerCallbackContext = null;

	public DriverAppPlugin() {
		this.receiver = null;
	}

	@Override
	public void initialize(CordovaInterface cordova, CordovaWebView webView) {
		super.initialize(cordova, webView);

		ctx = webView.getContext();
		Log.w(LOG_TAG, "context: " + ctx);

		Intent climbServiceIntent = new Intent(ctx, ClimbSimpleService.class);
		Log.w(LOG_TAG, "climbServiceIntent: " + climbServiceIntent);
		isBound = ctx.bindService(climbServiceIntent, mServiceConnection, Context.BIND_AUTO_CREATE);
		Log.w(LOG_TAG, "bound? " + isBound);
	}

	@Override
	public boolean execute(String action, JSONArray data, CallbackContext callbackContext) throws JSONException {
		Log.w(LOG_TAG, "action: " + action);

		if (action.equals("initialize")) {
			mClimbService.init();
			// TODO handle success
			callbackContext.success();
			return true;
		}

		if (action.equals("deinitialize")) {
			mClimbService.deinit();
			// TODO handle success
			callbackContext.success();
			return true;
		}

		if (action.equals("getNetworkState")) {
			NodeState[] networkState = mClimbService.getNetworkState();
			if (networkState == null) {
				Log.w(LOG_TAG, "getNetworkState: No master connected!");
				callbackContext.error("No master connected!");
			} else {
				JSONArray networkStateJSON = new JSONArray();
				for (int i = 0; i < networkState.length; i++) {
					networkStateJSON.put(nodeState2json(networkState[i]));
				}

				Log.w(LOG_TAG, "getNetworkState: " + networkStateJSON.toString());
				callbackContext.success(networkStateJSON);
			}

			return true;
		}
 
		//The asynchronous listener is left only for reference. The application do not uses it. 
		//Note that the listener won't do anything since no action is added.
		//To make it work add an action to the intent intentFilter.addAction(ClimbServiceInterface.STATE_CONNECTED_TO_CLIMB_MASTER);
		if (action.equals("startListener")) {
			if (this.listenerCallbackContext != null) {
				callbackContext.error("Already running.");
				return true;
			}
			this.listenerCallbackContext = callbackContext;

			IntentFilter intentFilter = new IntentFilter();
			//intentFilter.addAction(ClimbServiceInterface.STATE_CONNECTED_TO_CLIMB_MASTER);
			//intentFilter.addAction(ClimbServiceInterface.STATE_DISCONNECTED_FROM_CLIMB_MASTER);
			//intentFilter.addAction(ClimbServiceInterface.STATE_CHECKEDIN_CHILD);
			//intentFilter.addAction(ClimbServiceInterface.STATE_CHECKEDOUT_CHILD);

			if (this.receiver == null) {
				this.receiver = new BroadcastReceiver() {
					@Override
					public void onReceive(Context context, Intent intent) {
						sendUpdate(createUpdateJSONObject(intent), true);
					}
				};
				
				webView.getContext().registerReceiver(this.receiver, intentFilter);
			}

			// Don't return any result now, since status results will be sent
			// when events come in from broadcast receiver
			PluginResult pluginResult = new PluginResult(PluginResult.Status.NO_RESULT);
			pluginResult.setKeepCallback(true);
			callbackContext.sendPluginResult(pluginResult);
			return true;
		}

		if (action.equals("stopListener")) {
			removeListener();
			// release status callback in JS side
			this.sendUpdate(new JSONObject(), false);
			this.listenerCallbackContext = null;
			callbackContext.success();
			return true;
		}

		if (action.equals("getLogFiles")) {
			String[] logFilePaths = mClimbService.getLogFiles();

			if (logFilePaths.length > 0) {
				JSONArray logFilePathsJSON = new JSONArray();
				for (int i = 0; i < logFilePaths.length; i++) {
					logFilePathsJSON.put(logFilePaths[i]);
				}
				callbackContext.success(logFilePathsJSON);
			} else {
				callbackContext.error("getLogFiles error!");
			}
			return true;
		}

		if (action.equals("test")) {
			String name = data.getString(0);
			String message = "Hello, " + name;
			Log.w(LOG_TAG, "test: " + message);
			callbackContext.success(message);
			return true;
		}

		Log.w(LOG_TAG, action + ": error!");
		callbackContext.error("Error!");
		return false;
	}

	private JSONObject createUpdateJSONObject(Intent intent) {
		JSONObject obj = new JSONObject();

		try {
			obj.put("action", intent.getAction());
			obj.put("id", intent.getStringExtra(ClimbServiceInterface.INTENT_EXTRA_ID));
			if (intent.getBooleanExtra(ClimbServiceInterface.INTENT_EXTRA_SUCCESS, true) == false) {
				obj.put("errorMsg", intent.getStringExtra(ClimbServiceInterface.INTENT_EXTRA_MSG));
			}
		} catch (JSONException e) {
			Log.e(LOG_TAG, e.getMessage(), e);
		}

		return obj;
	}

	private void sendUpdate(JSONObject info, boolean keepCallback) {
		if (this.listenerCallbackContext != null) {
			PluginResult result = new PluginResult(PluginResult.Status.OK, info);
			result.setKeepCallback(keepCallback);
			this.listenerCallbackContext.sendPluginResult(result);
		}
	}

	public void onDestroy() {
		removeListener();

		if (isBound) {
			ctx.unbindService(mServiceConnection);
			Log.w(LOG_TAG, "unbindService called");
		}
	}

	public void onReset() {
		removeListener();
	}

	private void removeListener() {
		if (this.receiver != null) {
			try {
				webView.getContext().unregisterReceiver(this.receiver);
				this.receiver = null;
				Log.w(LOG_TAG, "Listener removed");
			} catch (Exception e) {
				Log.e(LOG_TAG, "Error unregistering receiver: " + e.getMessage(), e);
			}
		}
	}

	private final ServiceConnection mServiceConnection = new ServiceConnection() {
		@Override
		public void onServiceConnected(ComponentName className, IBinder service) {
			mClimbService = (ClimbSimpleService) ((ClimbSimpleService.LocalBinder) service).getService();
			// mClimbService.setHandler(new Handler());
			mClimbService.setContext(webView.getContext());
			Log.w(LOG_TAG, "climbService: " + mClimbService);
		}

		@Override
		public void onServiceDisconnected(ComponentName className) {
			mClimbService = null;
			Log.w(LOG_TAG, "climbService: " + mClimbService);
		}
	};

	/*
	 * Utils
	 */
	private JSONObject nodeState2json(NodeState node) {
		JSONObject jsonObject = null;

		try {
			jsonObject = new JSONObject();
			jsonObject.put("nodeID", node.nodeID);
			jsonObject.put("lastSeen", node.lastSeen);
			jsonObject.put("batteryLevel", node.batteryLevel);
			jsonObject.put("batteryVoltage_mV", node.batteryVoltage_mV);
			jsonObject.put("rssi", node.rssi);
		} catch (JSONException e) {
			Log.w(LOG_TAG, "nodeState2json: " + e.getMessage());
		}

		return jsonObject;
	}
}
