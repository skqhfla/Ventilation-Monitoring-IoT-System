<?php
    function change_service(){
        include 'inc_head.php';

        $host = /*DB IP*/
        $user = /*DB ID*/
        $pw = /*DB Password*/
        $dbName = /*DB Name*/

        $conn = new mysqli($host, $user, $pw, $dbName);

        $username = $_SESSION[ 'username' ];
        
        $sql = "UPDATE user_info SET service = 1 WHERE id = '$username'";

        $result = mysqli_query($conn, $sql);

        mysqli_close($conn);
        
        return $result;
    }
?>