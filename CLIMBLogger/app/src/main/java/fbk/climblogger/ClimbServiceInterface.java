package fbk.climblogger;

import android.content.Context;

import java.util.ArrayList;

public interface ClimbServiceInterface {

    public enum State {
        BYMYSELF(0), CHECKING(1), ONBOARD(2), ALERT(3), GOINGTOSLEEP(4), ERROR(5);
        private final int id;
        State(int id) { this.id = id; }
        public int getValue() { return id; }
    }

    public class NodeState{
        public String nodeID;
        public int state;
        public long lastSeen;   //local time in millisec
        public long lastStateChange; //local time in millisec
    }

    public final static String ACTION_DEVICE_ADDED_TO_LIST = "fbk.climblogger.ClimbService.ACTION_DEVICE_ADDED_TO_LIST";
    public final static String ACTION_DEVICE_REMOVED_FROM_LIST = "fbk.climblogger.ClimbService.ACTION_DEVICE_REMOVED_FROM_LIST";
    public final static String ACTION_METADATA_CHANGED ="fbk.climblogger.ClimbService.ACTION_METADATA_CHANGED";
    public final static String ACTION_NODE_ALERT ="fbk.climblogger.ClimbService.ACTION_NODE_ALERT";
    public final static String ACTION_DATA_AVAILABLE ="fbk.climblogger.ClimbService.ACTION_DATA_AVAILABLE";

    public final static String STATE_CONNECTED_TO_CLIMB_MASTER = "fbk.climblogger.ClimbService.STATE_CONNECTED_TO_CLIMB_MASTER";
    public final static String STATE_DISCONNECTED_FROM_CLIMB_MASTER = "fbk.climblogger.ClimbService.STATE_DISCONNECTED_FROM_CLIMB_MASTER";
    public final static String STATE_CHECKEDIN_CHILD = "fbk.climblogger.ClimbService.STATE_CHECKEDIN_CHILD";
    public final static String STATE_CHECKEDOUT_CHILD = "fbk.climblogger.ClimbService.STATE_CHECKEDOUT_CHILD";

    /*
     * NodeID in string format
     */
    public final static String INTENT_EXTRA_ID ="fbk.climblogger.ClimbService.INTENT_EXTRA_ID";
    /*
     * Success/failure as boolean
     */
    public final static String INTENT_EXTRA_SUCCESS ="fbk.climblogger.ClimbService.INTENT_EXTRA_SUCCESS";
    /*
     * Message describing failure reason in case of failure
     */
    public final static String INTENT_EXTRA_MSG ="fbk.climblogger.ClimbService.INTENT_EXTRA_MSG";

    public boolean init();
    public void setContext(Context context);
    public String[] getMasters();
    public boolean connectMaster(String master);
    public boolean disconnectMaster();

    /**
     * Get path of the log files. As a side effect, flush these files.
     *
     * @return absolute path of each log file, if there is any.
     */
    public String[] getLogFiles();

    /**
     * Set the list all nodes that might belong to this master, i.e. nodes for which the master can change state.
     *
     * @param children List of node IDs.
     * @return false if master is not connected or other error occurs
     */
    public boolean setNodeList(String[] children);

    /**
     * Get state of a single child node.
     *
     * @param id Id of node.
     * @return State if the node, if available, null otherwise.
     */
    public NodeState getNodeState(String id);

    /**
     * Get state of every child node seen by the master.
     * @return Array of node states.
     */
    public NodeState[] getNetworkState();

    public boolean checkinChild(String child);
    public boolean checkinChildren(String[] children);
    public boolean checkoutChild(String child);
    public boolean checkoutChildren(String[] children);

    ////public boolean ScheduleWakeUpCmd(int timeout_sec);
}
