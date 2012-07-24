<?php

require_once 'DB.php';

define('RESPONSE_OK', 200);
define('RESPONSE_NO_FUNDS', 402);
define('RESPONSE_DISABLED', 403);
define('RESPONSE_UNKNOWN_USER', 404);
define('RESPONSE_ERROR', 500);



$config = array(
    'db.dsn' => 'mysqli://kegtroller:password@localhost/kegtroller',
    'beer_cost' => 2.00,
    'email.from' => 'beer@machadolab.com',
    'email.bcc' => 'mike@machadolab.com',
    'debug' => 1,
);


// Dispatch controller
$actions = array(
    'auth' => 'auth',
    'info' => 'info',
    'paypal' => 'paypal',
    '_default_' => 'info',
);


$db =& DB::connect($config['db.dsn']);
if (PEAR::isError($db)) {
    die($db->getMessage());
}


$action = (empty($_REQUEST['a'])) ? '_default_' : $_REQUEST['a'];
if ($actions[$action]) {
    call_user_func($actions[$action]);
} else {
    echo "ERROR: Unknown action $action\n";
}



function renderResponse($code, $message="OK")
{
    header('HTTP/1.1 ' . $code . ' ' . $message);
    echo $code.':'.$message;
}

function auth()
{
    global $config, $db;

    $cardId = $_REQUEST['id'];
    $sql = "SELECT * FROM user WHERE card_id = " . $db->quoteSmart($cardId);

    $user = sql_getRow($sql);

    if ($user)
    {
        if ($user['disabled'])
        {
            renderResponse(RESPONSE_DISABLED, "Account disabled");
        }
        elseif ($user['balance'] >= $config['beer_cost'])
        {
            $sql = 'INSERT INTO transaction (user_id, amount, date_added, description) VALUES ('.
                $user['user_id'].','.
                ($config['beer_cost']*-1).',NOW(),"Beer purchase")';
            sql_insert($sql);

            $sql = 'UPDATE user SET balance = balance - '.$config['beer_cost'].' WHERE user_id = '.$user['user_id'];
            sql_update($sql);

            $subject = "You've got beer";
            $message = 'You just got yourself a beer. Your account has been debited by $'.
                sprintf('%.2f', $config['beer_cost']).' and your new balance is $'.
                sprintf('%.2f', ($user['balance'] - $config['beer_cost'])).'. Thanks for supporting kegtroller!';
            $headers = 'Bcc: '.$config['email.bcc']. "\r\n" .
                'From: '.$config['email.from']. "\r\n";

            mail($user['email'], $subject, $message, $headers, '-f'.$config['email.from']);

            renderResponse(RESPONSE_OK, "Drink Up");
        }
        else
        {
            renderResponse(RESPONSE_NO_FUNDS, "Not enough funds: " . $user['balance']);
        }
    }
    else
    {
        renderResponse(RESPONSE_UNKNOWN_USER, "Card ID not found");
    }

}


function info()
{
    echo phpinfo();
}



function paypal()
{
    global $config, $db;

    // read the post from PayPal system and add 'cmd'
    $req = 'cmd=' . urlencode('_notify-validate');

    foreach ($_POST as $key => $value) {
        $value = urlencode(stripslashes($value));
        $req .= "&$key=$value";
    }

    app_log("Received event from paypal: " . json_encode($_REQUEST));

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, 'https://www.paypal.com/cgi-bin/webscr');
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_POST, 1);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER,1);
    curl_setopt($ch, CURLOPT_POSTFIELDS, $req);
    curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 1);
    curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, 2);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Host: www.paypal.com'));
    $res = curl_exec($ch);
    curl_close($ch);


    // assign posted variables to local variables
    $payment_status = $_POST['payment_status'];
    $payment_amount = $_POST['mc_gross'];
    $payment_currency = $_POST['mc_currency'];
    $txn_id = $_POST['txn_id'];
    $receiver_email = $_POST['receiver_email'];
    $payer_email = $_POST['payer_email'];


    app_log("paypal payment result is " . $res);

    if (strcmp ($res, "VERIFIED") == 0)
    {

        if ($payment_status == 'Completed')
        {
            $sql = "SELECT * FROM user WHERE email = " . $db->quoteSmart($payer_email);
            $user = sql_getRow($sql);

            $userId = 0;
            if ($user && $user['user_id'])
            {
                $userId = $user['user_id'];
                $sql = 'UPDATE user SET balance = balance + ' . $payment_amount . ' WHERE user_id = ' . $userId;
                sql_update($sql);
            }
            else
            {
                $sql = 'INSERT INTO user (email, balance) VALUES ('.
                    $db->quoteSmart($payer_email).','.
                    $db->quoteSmart($payment_amount).')';
                $userId = sql_insert($sql);
            }
            $sql = 'INSERT INTO transaction (user_id, amount, date_added, description) VALUES ('.
                $userId.','.
                $db->quoteSmart($payment_amount).',NOW(),"Payment from paypal - transaction '.$txn_id.'")';
            sql_insert($sql);


            $subject = "Beer beer beer credit received";
            $message = 'Thanks for contributing to kegtroller! Your account has been credited by $'.
                sprintf('%.2f', $payment_amount).'. Drink up and be merry.';
            $headers = 'Bcc: '.$config['email.bcc']. "\r\n" .
                        'From: '.$config['email.from']. "\r\n";

            mail($payer_email, $subject, $message, $headers, '-f'.$config['email.from']);
        }
        else
        {
            app_log("Got incomplete payment?");
        }
        // check the payment_status is Completed
        // check that txn_id has not been previously processed
        // check that receiver_email is your Primary PayPal email
        // check that payment_amount/payment_currency are correct
        // process payment
    }
    else if (strcmp ($res, "INVALID") == 0)
    {
        app_log("INVALID payment from paypal: " . json_encode($_REQUEST));
    }
}


function sql_getAll($sql) {
    // returns associative array of first row returned from $sql statement. Should be select.
    global $db;
    app_log("SQL: $sql");

    $res =& $db->getAll($sql, array(), DB_FETCHMODE_ASSOC);

    if (DB::isError($res)) {
        app_log("SQL FAILED: " . $res->getMessage () . "\n");
        die ("SQL FAILED: " . $res->getMessage () . "\n");
    }

    return $res;
}

function sql_getRow($sql) {
    // returns associative array of first row returned from $sql statement. Should be select.
    global $db;
    app_log("SQL: $sql");

    $res =& $db->getRow($sql, array(), DB_FETCHMODE_ASSOC);

    if (DB::isError($res)) {
        app_log("SQL FAILED: " . $res->getMessage () . "\n");
        die ("SQL FAILED: " . $res->getMessage () . "\n");
    }

    return $res; // array or
}

function sql_insert($sql,$table=false) {
    global $db;
    app_log("SQL: $sql");

    $res =& $db->query($sql);
    if (DB::isError($res))
        die ("INSERT failed: " . $res->getMessage () . "\n"); // maybe silently fail, return false when live?

    if($table) {
        $id = $db->getOne("SELECT LAST_INSERT_ID() FROM " . $table); // _id for insert
        return $id;
    } else {
        return true;
    }
}

function sql_update($sql) {
    global $db;
    app_log("SQL: $sql");

    $res =& $db->query($sql);
    if (DB::isError($res))
        die ("UPDATE failed: " . $res->getMessage () . "\n"); // maybe silently fail, return false when live?
    return true;
}


function app_log($message, $debug = 0) {
    global $config;

    if ($config['debug']) {
        error_log($message);
    } else if ($debug == 0) {
        error_log($message);
    }
}
