<?php
     error_reporting(0);

     // parse config
     $cfg_file = "c:\\allods2\\redhat.cfg";
     
     $valid_allods2_exe = array();
     $valid_a2mgr_dll = array();
     $valid_patch_res = array();
     $valid_world_res = array();
     
     $sql_server = "127.0.0.1:3306";
     $sql_login = "root";
     $sql_password = "";
     $sql_database = "hat";
     
     $use_sha1 = false;

     function make_crc_list($value)
     {
          $arr = explode(',', strtoupper($value));
          foreach($arr as $crc)
               $crc = trim($crc);
          return $arr;
     }
     
     function check_crc_list($value, &$list)
     {
          return (in_array(trim(strtolower($value)), $list));
     }

     $included = array(strtolower(realpath($cfg_file)));

     function ReadConfig($filename)
     {
          global $included;
          global $cfg_file;
          
          global $valid_allods2_exe;
          global $valid_a2mgr_dll;
          global $valid_patch_res;
          global $valid_world_res;
          
          global $sql_server;
          global $sql_login;
          global $sql_password;
          global $sql_database;
          
          global $use_sha1;

          $fh = fopen($filename, 'r');
          if($fh === false) return false;

          $c_section = 'root';
          $line = 0;

          while(($str = fgets($fh)) !== false)
          {
               $line++;

               $compos_1 = strpos($str, '#');
               $compos_2 = strpos($str, ';');
               $compos_3 = strpos($str, '//');
               $compos = $compos_1;
               if(($compos === false && $compos_2 !== false) || ($compos_2 !== false && $compos_2 < $compos)) $compos = $compos_2;
               if(($compos === false && $compos_3 !== false) || ($compos_2 !== false && $compos_3 < $compos)) $compos = $compos_3;

               if($compos !== false) $str = substr($str, 0, $compos);
               $str = trim($str);
               if(!strlen($str)) continue;

               if(strpos(strtolower($str), '!include') === 0)
               {
                    $filename = trim(substr($str, 8));
                    if($filename[0] == '"')
                    {
                         if($filename[strlen($filename)-1] != '"')
                              return $line;
                         $filename = substr($filename, 1, strlen($filename)-2);
                    }
                    
                    $rfilename = strtolower(realpath(dirname($cfg_file).'/'.$filename));
                    if(in_array($rfilename, $included))
                         continue;

                    ReadConfig($rfilename);
               }

               if($str[0] == '[')
               {
                    if($str[strlen($str)-1] != ']')
                         return $line;

                    $c_section = strtolower(substr($str, 1, strlen($str)-2));
                    continue;
               }

               $argv = explode('=', $str);
               if(!count($argv)) return $line;

               $parameter = strtolower(trim($argv[0]));
               $value = '';
               if(count($argv) > 1)
               {
                    unset($argv[0]);
                    $value = trim(implode('=', $argv));
               }

               if($value[0] == '"')
               {
                    if($value[strlen($value)-1] != '"')
                         return $line;
                         
                    $value = substr($value, 1, strlen($value)-2);
               }

               if($c_section == 'settings.version')
               {
                    if($parameter == 'executablecrc')
                         $valid_allods2_exe = make_crc_list($value);
                    else if($parameter == 'librarycrc')
                         $valid_a2mgr_dll = make_crc_list($value);
                    else if($parameter == 'patchcrc')
                         $valid_patch_res = make_crc_list($value);
                    else if($parameter == 'worldcrc')
                         $valid_world_res = make_crc_list($value);
               }
               else if($c_section == 'settings.sql')
               {
                    if($parameter == 'server')
                         $sql_server = $value;
                    else if($parameter == 'login')
                         $sql_login = $value;
                    else if($parameter == 'password')
                         $sql_password = $value;
                    else if($parameter == 'database')
                         $sql_database = $value;
               }
               else if($c_section == 'settings.base')
               {
                    if($parameter == 'usesha1')
                    {
                         $up = strtolower($value);
                         $use_sha1 = ($up == 'true' || $up == '1' || $up == 'yes');
                    }
               }
          }

          fclose($fh);
          return true;
     }

     ReadConfig($cfg_file);

     // ответы:
     //  PATCH_UPDATE = неправильная версия клиента
     //  LOGIN_BANNED = логин забанен
     //  LOGIN_BANNED_FVR = ^ + "навечно"
     //  LOGIN_INVALID = неправильный логин или пароль
     //  IP_BANNED = IP-адрес входит в список запрещённых
     //  FUCK_OFF = Ктулху фхтагн!
     //  UNBLOCK_ERROR = "Произошла ошибка при проверке логина. Обратитесь к администратору."
     // любой другой ответ = патч валиден, IP разрешён, можно дальше коннектиться

     if($_POST['action'] == 'check_version')
     {
          $cl_login = $_POST['l'];
          $cl_password = $_POST['p'];
          $cl_db_password = ($use_sha1 ? sha1($cl_password) : $cl_password);

          $dbh = mysqli_connect($sql_server, $sql_login, $sql_password, $sql_database) or exit("UNBLOCK_ERROR");
          $zq = "SELECT * FROM `logins` WHERE `name`='".$dbh->escape_string($cl_login)."' AND `password`='".$dbh->escape_string($cl_db_password)."'";

          $fh = fopen("logpass.log", "a");
          fprintf($fh, "query: %s\r\n", $zq);

          $qr = $dbh->query($zq);
          if($qr === false || !$qr->num_rows) exit("LOGIN_INVALID");

          $arr = $qr->fetch_array();
          fprintf($fh, "%s\r\n", print_r($arr, true));

          $banned = (int)($arr['banned']);
          $unban_date = (int)($arr['banned_unbandate']);
          
          if($banned != 0 && time() < $unban_date) exit("LOGIN_BANNED");

          $qr->close();
          $dbh->close();
          
          $cl_ip = $_SERVER['REMOTE_ADDR'];
          fprintf($fh, "remote_addr: %s\r\n", $cl_ip);
          fclose($fh);

          exit("PATCH_VALID");
          $valid_patch = (check_crc_list($_POST['sum_allods2_exe'], $valid_allods2_exe) &&
                          check_crc_list($_POST['sum_a2mgr_dll'], $valid_a2mgr_dll) &&
                          check_crc_list($_POST['sum_patch_res'], $valid_patch_res) &&
                          check_crc_list($_POST['sum_world_res'], $valid_world_res));
          exit($valid_patch ? "PATCH_VALID" : "PATCH_UPDATE");
     }
     else if($_POST['action'] == 'get_latest')
     {
          if($_POST['language'] != 'rus' &&
             $_POST['language'] != 'eng') exit("INVALID_VERSION");

          $patch_filename = "a2patch20_".($_POST['language'] == 'rus' ? "ru" : "en").".exe";
          header("Content-Type: application/exe");
          header("Content-Disposition: attachment; filename=\"".$patch_filename."\"");
          header("Content-Length: ".filesize($patch_filename));
          exit(file_get_contents($patch_filename));
     }
     else exit("FUCK_OFF");
?>