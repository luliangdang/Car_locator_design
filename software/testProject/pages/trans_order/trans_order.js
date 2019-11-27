// pages/trans_order/trans_order.js
var util = require('../../utils/util.js');
import { Config } from '../../utils/config.js';
var latitude = 0;
var longitude = 0;
Page({
  data: {
    order:{},
    img: '',
    coor:{},
    wxlatitude: 0,
    wxlongitude: 0,
    flag: 0
  },

  onLoad: function (options) {
    console.log(options);
    this.getLtransImg();
    this.gettime();
    var that = this;
    wx.request({
      url: Config.restUrl+'/api/v1/manager/getcarnoworder/'+options.cid, 
      header: {
        'content-type': 'application/json' // 默认值
      },
      success(res) {
        that.setData({
          flag:0
        });
        var dataxx = res.data;
        console.log('我在这',res.data);
        that.setData2page(res.data)
      }
    })
  },

  gettime:function(){
    var time = util.formatTime(new Date());
    console.log(time);
  },

  getLtransImg: function () {
    var that = this;
    wx.request({
      url: Config.restUrl + '/api/v1/mix/getimg/2',
      data: '',
      header: {
        'content-type': 'application/json' // 默认值
      },
      method: 'get',
      success(res) {
        var dataxx = Config.restUrl + '/img' + res.data;
        that.setData({
          img: dataxx
        });
      }
    })
  },

  pointToLook:function(){
    if(this.data.flag == 1){
      //latitude = this.order.car.car_position_wd;
      //longitude = this.order.car.car_position_jd;
      //console.log(this.order.car);
      console.log(latitude);
      console.log(longitude);
      wx.openLocation({
        latitude,
        longitude,
        scale: 10
      })
    }
        
  },

  setData2page: function(data){
    console.log("setData");
    console.log(data);
    latitude = Number(data.car.car_position_wd);
    longitude = Number(data.car.car_position_jd);
    this.setData({
      order:data,
      flag:1
    });
  },
  
  /*
  getOrderCoor:function(){
    var that = this;
    wx.request({
      url: Config.restUrl + '/api/v1/iotali/qdpst?productKey=a10uSuk2n4h&deviceName=9t9w4OkHECdV60myHviM',
      data: '',
      header: {
        'content-type': 'application/json' // 默认值
      },
      method: 'get',
      success(res) {
        console.log(res.data)
        console.log("赋值完成")
        var tempxxx = res.data.Data.List.PropertyStatusInfo;
        var arr6 = [];
        for (var i in tempxxx) {
          var arr7 = [];
          arr7.push(i);
          for (var j in tempxxx[i]) {
            arr7.push(tempxxx[i][j]);
          }
          arr6.push(arr7);
        }
        tempxxx = arr6[0][2];
        console.log('打印数组')
        //JSON.parse是把string转换成js object
        //JSON.stringify反过来把一个对象转换成string
        //var test = {a:1,b:2};
        //console.log("test",JSON.stringify(test));
        var resxxx = JSON.parse(tempxxx);
        console.log("res",resxxx);
        that.setData({
          coor: resxxx,
          wxlatitude: resxxx.Latitude,
          wxlongitude: resxxx.Longitude,
          flag: 1,
        });
        console.log('赋值完成');
        console.log(that.data.wxlatitude);
        console.log(that.data.wxlongitude);
        console.log(that.data.flag);
      }
    })
  },
  */

  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {

  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow: function () {

  },

  /**
   * 生命周期函数--监听页面隐藏
   */
  onHide: function () {

  },

  /**
   * 生命周期函数--监听页面卸载
   */
  onUnload: function () {

  },

  /**
   * 页面相关事件处理函数--监听用户下拉动作
   */
  onPullDownRefresh: function () {

  },

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {

  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})